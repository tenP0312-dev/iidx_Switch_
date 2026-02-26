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
    // Switch用のパス補正
    if (targetPath.compare(0, 5, "sdmc:") == 0) targetPath.erase(0, 5);

    // すでに再生中なら一旦リセット
    if (isVideoMode) clear();

    // 1. ファイルオープン
    int err = avformat_open_input(&pFormatCtx, targetPath.c_str(), NULL, NULL);
    if (err != 0) {
        // フォールバック: ROOT/videos/ フォルダ内を探す
        std::string filename = targetPath;
        size_t lastSlash = targetPath.find_last_of("/\\");
        if (lastSlash != std::string::npos) filename = targetPath.substr(lastSlash + 1);
        
        std::string fallbackPath = Config::ROOT_PATH;
        if (!fallbackPath.empty() && fallbackPath.back() != '/' && fallbackPath.back() != '\\') fallbackPath += "/";
        fallbackPath += "videos/" + filename;
        if (fallbackPath.compare(0, 5, "sdmc:") == 0) fallbackPath.erase(0, 5);
        
        err = avformat_open_input(&pFormatCtx, fallbackPath.c_str(), NULL, NULL);
    }
    if (err != 0) return false;

    // 2. ストリーム情報の取得
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) return false;

    // ビデオストリームの探索
    videoStreamIdx = -1;
    for (int i = 0; i < (int)pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            break;
        }
    }
    if (videoStreamIdx == -1) return false;

    // 3. デコーダーの設定
    AVCodecParameters* pCodecPar = pFormatCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec* pCodec = avcodec_find_decoder(pCodecPar->codec_id);
    if (!pCodec) return false;

    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pCodecPar);
    
    // --- Switch最適化設定 ---
    pCodecCtx->thread_count = 2; // デコード用スレッド数
    pCodecCtx->flags2 |= AV_CODEC_FLAG2_FAST; // 高速化フラグ
    pCodecCtx->workaround_bugs = 1;

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) return false;

    // 4. SDLテクスチャとフレームの準備
    pFrame = av_frame_alloc();
    videoTexture = SDL_CreateTexture(renderer, 
                                     SDL_PIXELFORMAT_IYUV, 
                                     SDL_TEXTUREACCESS_STREAMING, 
                                     pCodecCtx->width, 
                                     pCodecCtx->height);

    // 5. デコードスレッドの開始
    quitThread = false;
    isVideoMode = true;
    
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frameQueue.clear(); 
    }
    
    hasNewFrameToUpload = false;
    
    // スレッド起動
    decodeThread = std::thread(&BgaManager::videoWorker, this);

    // --- 修正: 事前ロード待機ロジック ---
    // 目標: 再生開始前に 30枚 (約1秒分) をメモリに貯金する
    // 最大 2秒間 (200 * 10ms) だけロード画面で待機
    int waitCount = 0;
    while (waitCount < 200) {
        size_t currentSize = 0;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            currentSize = frameQueue.size();
        }

        if (currentSize >= 60) {
            // 十分に溜まったのでプレイ画面へ遷移
            break; 
        }

        // 貯金が溜まるまで10msずつ待つ
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

// --- 修正: バッファリングを行うワーカー ---
void BgaManager::videoWorker() {
    AVPacket packet;
    while (!quitThread) {
        // 1. キューが満杯なら少し待機 (Producer側の制御)
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

                        // フレームデータをコピーして作成
                        VideoFrame vFrame;
                        vFrame.pts = frameTime;
                        vFrame.yStride = pFrame->linesize[0];
                        vFrame.uStride = pFrame->linesize[1];
                        vFrame.vStride = pFrame->linesize[2];

                        size_t ySize = pCodecCtx->height * vFrame.yStride;
                        size_t uSize = (pCodecCtx->height / 2) * vFrame.uStride;
                        size_t vSize = (pCodecCtx->height / 2) * vFrame.vStride;

                        vFrame.yBuf.resize(ySize);
                        vFrame.uBuf.resize(uSize);
                        vFrame.vBuf.resize(vSize);

                        memcpy(vFrame.yBuf.data(), pFrame->data[0], ySize);
                        memcpy(vFrame.uBuf.data(), pFrame->data[1], uSize);
                        memcpy(vFrame.vBuf.data(), pFrame->data[2], vSize);

                        // キューに追加
                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameQueue.push_back(std::move(vFrame));
                        }
                    }
                }
            }
            av_packet_unref(&packet);
        } else {
            // ループ再生
            av_seek_frame(pFormatCtx, videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(pCodecCtx);
            {
                std::lock_guard<std::mutex> lock(frameMutex);
                frameQueue.clear(); // ループ時はキューをクリアしてリセット
            }
        }
    }
}

// --- 修正: キューから取り出して描画 ---
void BgaManager::render(long long currentPulse, SDL_Renderer* renderer, int x, int y, double cur_ms) {
    // 1. 座標計算
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

    // BGAイベント処理
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

    // --- 動画更新ロジック (Consumer) ---
    if (isVideoMode && videoTexture) {
        double currentTime = sharedVideoElapsed.load(std::memory_order_acquire);
        
        // キューから最適なフレームを探す
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            
            while (!frameQueue.empty()) {
                VideoFrame& front = frameQueue.front();
                
                // 次のフレームもすでに現在時刻を過ぎているなら、今の先頭は古すぎるので捨てる（ドロップフレーム）
                if (frameQueue.size() >= 2 && frameQueue[1].pts <= currentTime) {
                    frameQueue.pop_front();
                    continue;
                }
                
                // 先頭フレームが現在時刻に到達した（あるいはわずかに過ぎた）なら採用
                if (front.pts <= currentTime + 0.05) { // 0.05秒程度の許容誤差
                    // データをコピーしてテクスチャ更新フラグを立てる
                    currentY = front.yBuf;
                    currentU = front.uBuf;
                    currentV = front.vBuf;
                    currentYStride = front.yStride;
                    currentUStride = front.uStride;
                    currentVStride = front.vStride;
                    hasNewFrameToUpload = true;
                    
                    // 使い終わったフレームを捨てる
                    frameQueue.pop_front();
                }
                break; 
            }
        }

        if (hasNewFrameToUpload) {
            SDL_UpdateYUVTexture(videoTexture, NULL,
                                 currentY.data(), currentYStride,
                                 currentU.data(), currentUStride,
                                 currentV.data(), currentVStride);
            hasNewFrameToUpload = false;
        }
        SDL_RenderCopy(renderer, videoTexture, NULL, &dst);
    } else {
        if (lastDisplayedId != -1 && textures.count(lastDisplayedId)) SDL_RenderCopy(renderer, textures[lastDisplayedId], NULL, &dst);
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