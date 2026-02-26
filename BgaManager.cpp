#include "BgaManager.hpp"
#include "Config.hpp"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <thread>

// --- 定数の定義 ---
static const long long BMP_LOOK_AHEAD = 300000;

void BgaManager::init(size_t expectedSize) {
    clear();
    textures.reserve(std::min((size_t)256, expectedSize));
}

bool BgaManager::loadBgaFile(const std::string& path, SDL_Renderer* renderer) {
    std::string targetPath = path;
    if (targetPath.compare(0, 5, "sdmc:") == 0) targetPath.erase(0, 5);

    if (isVideoMode) clear();

    int err = avformat_open_input(&pFormatCtx, targetPath.c_str(), NULL, NULL);
    if (err != 0) {
        std::string filename = targetPath;
        size_t lastSlash = targetPath.find_last_of("/\\");
        if (lastSlash != std::string::npos) filename = targetPath.substr(lastSlash + 1);
        std::string fallbackPath = Config::ROOT_PATH + "videos/" + filename;
        if (fallbackPath.compare(0, 5, "sdmc:") == 0) fallbackPath.erase(0, 5);
        err = avformat_open_input(&pFormatCtx, fallbackPath.c_str(), NULL, NULL);
    }
    if (err != 0) return false;

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) return false;

    videoStreamIdx = -1;
    for (int i = 0; i < (int)pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            break;
        }
    }
    if (videoStreamIdx == -1) return false;

    AVCodecParameters* pCodecPar = pFormatCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec* pCodec = avcodec_find_decoder(pCodecPar->codec_id);
    if (!pCodec) return false;

    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pCodecPar);
    pCodecCtx->thread_count = 2;
    pCodecCtx->flags2 |= AV_CODEC_FLAG2_FAST;
    pCodecCtx->workaround_bugs = 1;

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) return false;

    pFrame = av_frame_alloc();

    // ★修正：IYUV -> NV12 に変更（SwitchのGPUと親和性が高く、転送が高速）
    videoTexture = SDL_CreateTexture(renderer, 
                                     SDL_PIXELFORMAT_NV12, 
                                     SDL_TEXTUREACCESS_STREAMING, 
                                     pCodecCtx->width, 
                                     pCodecCtx->height);

    quitThread = false;
    isVideoMode = true;
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frameQueue.clear(); 
    }
    hasNewFrameToUpload = false;
    decodeThread = std::thread(&BgaManager::videoWorker, this);

    int waitCount = 0;
    while (waitCount < 200) {
        size_t currentSize = 0;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            currentSize = frameQueue.size();
        }
        if (currentSize >= 60) break; 
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waitCount++;
    }

    return true;
}

void BgaManager::preLoad(long long startPulse, SDL_Renderer* renderer) {
    if (isVideoMode) return; 
    int nextNeededId = -1;
    auto scan = [&](const std::vector<BgaEvent>& events) {
        for (const auto& ev : events) {
            if (ev.y > startPulse + BMP_LOOK_AHEAD) break;
            if (textures.find(ev.id) == textures.end()) {
                nextNeededId = ev.id;
                return true;
            }
        }
        return false;
    };
    if (!scan(bgaEvents)) { if (!scan(layerEvents)) scan(poorEvents); }
    if (nextNeededId != -1 && idToFilename.count(nextNeededId)) {
        loadBmp(nextNeededId, baseDir + idToFilename[nextNeededId], renderer);
    }
}

void BgaManager::loadBmp(int id, const std::string& fullPath, SDL_Renderer* renderer) {
    if (textures.count(id)) return;
    SDL_Surface* surf = IMG_Load(fullPath.c_str());
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (tex) textures[id] = tex;
}

void BgaManager::syncTime(double ms) {
    sharedVideoElapsed.store(ms / 1000.0, std::memory_order_release);
}

// --- 修正: バッファリングを行うワーカー（脱malloc・リングバッファ版） ---
void BgaManager::videoWorker() {
    AVPacket packet;
    
    // 1. バッファプールをあらかじめ一括確保 (SwitchのL2キャッシュ・メモリ断片化対策)
    // 毎フレームの malloc/free を完全に排除するため、初期化時に全フレーム分の領域を確保
    size_t ySize = pCodecCtx->height * pCodecCtx->width; 
    size_t uvSize = (pCodecCtx->height / 2) * (pCodecCtx->width / 2);
    
    // MAX_FRAME_QUEUE 分のメモリを事前にリザーブ (動的な resize を防止)
    std::vector<uint8_t> memoryPoolY(ySize * MAX_FRAME_QUEUE);
    std::vector<uint8_t> memoryPoolU(uvSize * MAX_FRAME_QUEUE);
    std::vector<uint8_t> memoryPoolV(uvSize * MAX_FRAME_QUEUE);
    
    // リングバッファ用の書き込みインデックス
    size_t writeIdx = 0;

    while (!quitThread) {
        // キューの空き容量確認
        bool isFull = false;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            isFull = (frameQueue.size() >= MAX_FRAME_QUEUE);
        }
        if (isFull) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // 2. デコード処理
        if (av_read_frame(pFormatCtx, &packet) >= 0) {
            if (packet.stream_index == videoStreamIdx) {
                if (avcodec_send_packet(pCodecCtx, &packet) >= 0) {
                    while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
                        int64_t pts = pFrame->best_effort_timestamp;
                        if (pts == AV_NOPTS_VALUE) pts = 0;
                        int64_t startTime = pFormatCtx->streams[videoStreamIdx]->start_time;
                        if (startTime != AV_NOPTS_VALUE) pts -= startTime;
                        
                        double frameTime = pts * av_q2d(pFormatCtx->streams[videoStreamIdx]->time_base);

                        // --- 【最適化】プリロード済みバッファへの直接コピー (No-Alloc) ---
                        VideoFrame vFrame;
                        vFrame.pts = frameTime;
                        vFrame.yStride = pFrame->linesize[0];
                        vFrame.uStride = pFrame->linesize[1];
                        vFrame.vStride = pFrame->linesize[2];

                        // プール内の固定位置をポインタとして割り当て
                        vFrame.yPtr = &memoryPoolY[writeIdx * ySize];
                        vFrame.uPtr = &memoryPoolU[writeIdx * uvSize];
                        vFrame.vPtr = &memoryPoolV[writeIdx * uvSize];

                        // IYUVプレーンごとのデータサイズを計算
                        size_t currentYSize = pCodecCtx->height * vFrame.yStride;
                        size_t currentUSize = (pCodecCtx->height / 2) * vFrame.uStride;
                        size_t currentVSize = (pCodecCtx->height / 2) * vFrame.vStride;

                        // 固定領域へ上書きコピー
                        memcpy(vFrame.yPtr, pFrame->data[0], std::min(currentYSize, ySize));
                        memcpy(vFrame.uPtr, pFrame->data[1], std::min(currentUSize, uvSize));
                        memcpy(vFrame.vPtr, pFrame->data[2], std::min(currentVSize, uvSize));

                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            // HPP側で frameQueue が固定長リングバッファ構造になっていない場合でも、
                            // この実装により内部の vector<uint8_t> 伸縮コストはゼロになります
                            frameQueue.push_back(vFrame);
                        }

                        // 書き込み位置を回す
                        writeIdx = (writeIdx + 1) % MAX_FRAME_QUEUE;
                    }
                }
            }
            av_packet_unref(&packet);
        } else {
            // ★修正：ループ再生用シーク処理を削除
            // 動画の終端に達した後は、デコードスレッドを終了させずに待機状態を維持する。
            // これにより、再生終了時の最後のフレームが render 関数側で維持されます。
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// --- 修正: キューから取り出して描画（ポインタアクセス版） ---
void BgaManager::render(long long currentPulse, SDL_Renderer* renderer, int x, int y, double cur_ms) {
    int sw = Config::SCRATCH_WIDTH;
    int lw = Config::LANE_WIDTH;
    int totalKeysWidth = 0;
    for (int i = 1; i <= 7; i++) totalKeysWidth += (i % 2 != 0) ? (int)(lw * 1.4) : lw;
    int totalWidth = totalKeysWidth + sw; 
    int startX = (Config::PLAY_SIDE == 1) ? 50 : (Config::SCREEN_WIDTH - totalWidth - 50);
    int dynamicCenterX = (Config::PLAY_SIDE == 1) ? (startX + totalWidth + (Config::SCREEN_WIDTH - (startX + totalWidth)) / 2) : (startX / 2);
    
    int renderH = 512;
    int renderW = 512;
    
    SDL_Texture* targetTex = nullptr;
    if (isVideoMode && videoTexture) targetTex = videoTexture;
    else if (lastDisplayedId != -1 && textures.count(lastDisplayedId)) targetTex = textures[lastDisplayedId];

    if (targetTex) {
        int texW, texH;
        SDL_QueryTexture(targetTex, NULL, NULL, &texW, &texH);
        if (texH > 0) renderW = (int)(512.0f * ((float)texW / (float)texH));
    }
    
    SDL_Rect dst = { dynamicCenterX - (renderW / 2), (Config::SCREEN_HEIGHT / 2) - (renderH / 2), renderW, renderH };

    while (currentEventIndex < bgaEvents.size() && bgaEvents[currentEventIndex].y <= currentPulse) {
        lastDisplayedId = bgaEvents[currentEventIndex].id;
        currentEventIndex++;
    }
    while (currentLayerIndex < layerEvents.size() && layerEvents[currentLayerIndex].y <= currentPulse) {
        lastLayerId = layerEvents[currentLayerIndex].id;
        currentLayerIndex++;
    }
    while (currentPoorIndex < poorEvents.size() && poorEvents[currentPoorIndex].y <= currentPulse) {
        lastPoorId = poorEvents[currentPoorIndex].id;
        currentPoorIndex++;
    }

    if (isVideoMode && videoTexture) {
        double currentTime = sharedVideoElapsed.load(std::memory_order_acquire);
        
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            while (!frameQueue.empty()) {
                VideoFrame& front = frameQueue.front();
                if (frameQueue.size() >= 2 && frameQueue[1].pts <= currentTime) {
                    frameQueue.pop_front();
                    continue;
                }
                
                if (front.pts <= currentTime + 0.05) {
                    // ★修正：低レイヤAPI (Lock/Unlock) を使用したNV12転送
                    void* pixels;
                    int pitch;
                    if (SDL_LockTexture(videoTexture, NULL, &pixels, &pitch) == 0) {
                        uint8_t* dstY = (uint8_t*)pixels;
                        uint8_t* dstUV = dstY + (pitch * pCodecCtx->height);

                        // 1. Y面のコピー
                        for (int i = 0; i < pCodecCtx->height; ++i) {
                            memcpy(dstY + i * pitch, front.yPtr + i * front.yStride, pCodecCtx->width);
                        }

                        // 2. UV面のコピー (NV12形式)
                        // ※本来はworkerでNV12化すべきだが、既存ロジック100%継承のためここで簡易合成
                        for (int i = 0; i < pCodecCtx->height / 2; ++i) {
                            uint8_t* dUV = dstUV + i * pitch;
                            uint8_t* sU = front.uPtr + i * front.uStride;
                            uint8_t* sV = front.vPtr + i * front.vStride;
                            for (int j = 0; j < pCodecCtx->width / 2; ++j) {
                                dUV[j * 2] = sU[j];
                                dUV[j * 2 + 1] = sV[j];
                            }
                        }
                        SDL_UnlockTexture(videoTexture);
                    }
                    frameQueue.pop_front();
                }
                break; 
            }
        }
        SDL_RenderCopy(renderer, videoTexture, NULL, &dst);
    } else {
        if (lastDisplayedId != -1 && textures.count(lastDisplayedId)) {
            SDL_RenderCopy(renderer, textures[lastDisplayedId], NULL, &dst);
        }
    }

    if (lastLayerId != -1 && textures.count(lastLayerId)) SDL_RenderCopy(renderer, textures[lastLayerId], NULL, &dst);
    if (showPoor && lastPoorId != -1 && textures.count(lastPoorId)) SDL_RenderCopy(renderer, textures[lastPoorId], NULL, &dst);
}

void BgaManager::clear() {
    quitThread = true;
    if (decodeThread.joinable()) decodeThread.join();
    for (auto& pair : textures) if (pair.second) SDL_DestroyTexture(pair.second);
    textures.clear();
    if (videoTexture) { SDL_DestroyTexture(videoTexture); videoTexture = nullptr; }
    if (pFrame) { av_frame_free(&pFrame); pFrame = nullptr; }
    if (pCodecCtx) { avcodec_free_context(&pCodecCtx); pCodecCtx = nullptr; }
    if (pFormatCtx) { avformat_close_input(&pFormatCtx); pFormatCtx = nullptr; }
    
    isVideoMode = false;
    hasNewFrameToUpload = false;
    frameQueue.clear();
    currentEventIndex = 0; currentLayerIndex = 0; currentPoorIndex = 0;
    lastDisplayedId = -1; lastLayerId = -1; lastPoorId = -1;
}

void BgaManager::cleanup() { clear(); }
