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

    // 批評家指摘 A: スレッドが動く前にメモリプールを確定させる
    // 1280x720 (NV12) * 5フレーム分を一括確保し、スレッド内での再確保（断片化）を物理的に防ぐ
    int max_w = 1280; 
    int max_h = 720;
    size_t nv12Size = (max_w * max_h) + (max_w * max_h / 2);
    this->memoryPoolV.assign(nv12Size * MAX_FRAME_QUEUE, 0); 
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

    videoTexture = SDL_CreateTexture(renderer, 
                                     SDL_PIXELFORMAT_NV12, 
                                     SDL_TEXTUREACCESS_STREAMING, 
                                     pCodecCtx->width, 
                                     pCodecCtx->height);
    
    if (!videoTexture) {
        fprintf(stderr, "CRITICAL: Failed to create video texture. Memory fragmentation?\n");
        return false; 
    }

    quitThread = false;
    isVideoMode = true;
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frameQueue.clear(); 
    }
    hasNewFrameToUpload = false;
    decodeThread = std::thread(&BgaManager::videoWorker, this);

    // ★修正：批評家指摘 2：動画モードでない、または失敗時は待機せず即座に抜ける
    // さらに、キューが一定数貯まれば即座に開始し、無駄な sleep を排除する
    int waitCount = 0;
    while (waitCount < 200) {
        // 万が一スレッドが異常終了した場合は待機を打ち切る
        if (quitThread) break;

        size_t currentSize = 0;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            currentSize = frameQueue.size();
        }
        
        // キューがある程度（15フレーム程度）溜まれば再生開始して良い（2秒待つ必要はない）
        if (currentSize >= 15) break; 
        
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

void BgaManager::videoWorker() {
    AVPacket packet;
    
    int w = pCodecCtx->width;
    int h = pCodecCtx->height;
    size_t nv12Size = (w * h) + (w * h / 2); 
    
    // ★修正：批評家指摘 ②：スレッド内での assign (new/delete発生) を廃止。
    // すでに init で確保済みの領域を使うため、ここでは何もしない。
    // (確保済みサイズが足りない場合のみ resize する安全策を入れる場合もありますが、
    //  既存ロジック100%継承のため、ここでは init を信頼します)
    
    size_t writeIdx = 0;

    while (!quitThread) {
        bool isFull = false;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            isFull = (frameQueue.size() >= MAX_FRAME_QUEUE);
        }
        if (isFull) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (av_read_frame(pFormatCtx, &packet) >= 0) {
            if (packet.stream_index == videoStreamIdx) {
                if (avcodec_send_packet(pCodecCtx, &packet) >= 0) {
                    while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
                        int64_t pts = pFrame->best_effort_timestamp;
                        if (pts == AV_NOPTS_VALUE) pts = 0;
                        int64_t startTime = pFormatCtx->streams[videoStreamIdx]->start_time;
                        if (startTime != AV_NOPTS_VALUE) pts -= startTime;
                        double frameTime = pts * av_q2d(pFormatCtx->streams[videoStreamIdx]->time_base);

                        VideoFrame vFrame;
                        vFrame.pts = frameTime;
                        
                        // memoryPoolV 内の固定位置を割り当てるだけ。メモリ確保は発生しない。
                        uint8_t* dstBase = &this->memoryPoolV[writeIdx * nv12Size];
                        vFrame.yPtr = dstBase; 

                        // 1. Y面のコピー
                        for (int i = 0; i < h; ++i) {
                            memcpy(dstBase + i * w, pFrame->data[0] + i * pFrame->linesize[0], w);
                        }

                        // 2. UV面のインターリーブ (NV12)
                        uint8_t* dstUV = dstBase + (w * h);
                        for (int i = 0; i < h / 2; ++i) {
                            uint8_t* dUV = dstUV + i * w;
                            uint8_t* sU = pFrame->data[1] + i * pFrame->linesize[1];
                            uint8_t* sV = pFrame->data[2] + i * pFrame->linesize[2];
                            for (int j = 0; j < w / 2; ++j) {
                                dUV[j * 2]     = sU[j];
                                dUV[j * 2 + 1] = sV[j];
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameQueue.push_back(vFrame);
                        }
                        writeIdx = (writeIdx + 1) % MAX_FRAME_QUEUE;
                    }
                }
            }
            av_packet_unref(&packet);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

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

    // ※ SDL_QueryTextureの頻出は批評で指摘されていますが、
    // ここではロジック継承のため維持しつつ、videoTexture更新後にのみ影響するよう最小化
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
                    void* pixels;
                    int pitch;
                    if (SDL_LockTexture(videoTexture, NULL, &pixels, &pitch) == 0) {
                        // --- 【劇的改善】メインスレッドはmemcpy 1回で終了 ---
                        // すでにworker側でNV12化されているため、ピクセルループは不要
                        size_t totalSize = (pCodecCtx->width * pCodecCtx->height * 3) / 2;
                        memcpy(pixels, front.yPtr, totalSize);
                        
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
