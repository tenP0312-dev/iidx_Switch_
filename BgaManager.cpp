#include "BgaManager.hpp"
#include "Config.hpp"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <thread>

// --- 定数の定義 ---
std::deque<VideoFrame> BgaManager::freeQueue;
uint8_t* BgaManager::frameMemoryPool = nullptr;
static const long long BMP_LOOK_AHEAD = 300000;

void BgaManager::init(size_t expectedSize) {
    clear(); // 既存のテクスチャやデコーダーをリセット
    textures.reserve(std::min((size_t)256, expectedSize));

    // --- Switch最適化: プールがなければ一括確保 ---
    const size_t maxW = 1920;
    const size_t maxH = 1080;
    const size_t frameSize = maxW * maxH * 3 / 2; // YUV420P

    if (!frameMemoryPool) {
        frameMemoryPool = (uint8_t*)std::malloc(frameSize * MAX_FRAME_QUEUE);
    }

    // 利用可能なバッファをキューにセット（初期化）
    std::lock_guard<std::mutex> lock(frameMutex);
    freeQueue.clear();
    frameQueue.clear();
    for (int i = 0; i < (int)MAX_FRAME_QUEUE; ++i) {
        VideoFrame vf;
        vf.yBuf = frameMemoryPool + (frameSize * i);
        vf.uBuf = vf.yBuf + (maxW * maxH);
        vf.vBuf = vf.uBuf + (maxW * maxH / 4);
        vf.pts = -1.0;
        freeQueue.push_back(vf);
    }
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
    
    // バッファの状態をリセット（プール自体はfreeしない）
    std::lock_guard<std::mutex> lock(frameMutex);
    frameQueue.clear();
    freeQueue.clear(); 

    currentEventIndex = 0; currentLayerIndex = 0; currentPoorIndex = 0;
    lastDisplayedId = -1; lastLayerId = -1; lastPoorId = -1;
}

void BgaManager::cleanup() {
    clear();
    if (frameMemoryPool) {
        std::free(frameMemoryPool);
        frameMemoryPool = nullptr;
    }
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
        
        std::string fallbackPath = Config::ROOT_PATH;
        if (!fallbackPath.empty() && fallbackPath.back() != '/' && fallbackPath.back() != '\\') fallbackPath += "/";
        fallbackPath += "videos/" + filename;
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
                                     SDL_PIXELFORMAT_IYUV, 
                                     SDL_TEXTUREACCESS_STREAMING, 
                                     pCodecCtx->width, 
                                     pCodecCtx->height);

    quitThread = false;
    isVideoMode = true;
    
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frameQueue.clear(); 
    }
    
    decodeThread = std::thread(&BgaManager::videoWorker, this);

    // 事前貯金待機
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

void BgaManager::videoWorker() {
    AVPacket packet;
    while (!quitThread) {
        VideoFrame vFrame;
        bool hasFreeBuffer = false;

        {
            std::lock_guard<std::mutex> lock(frameMutex);
            if (!freeQueue.empty() && frameQueue.size() < MAX_FRAME_QUEUE) {
                vFrame = freeQueue.front();
                freeQueue.pop_front();
                hasFreeBuffer = true;
            }
        }

        if (!hasFreeBuffer) {
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
                        
                        vFrame.pts = pts * av_q2d(pFormatCtx->streams[videoStreamIdx]->time_base);
                        vFrame.yStride = pFrame->linesize[0];
                        vFrame.uStride = pFrame->linesize[1];
                        vFrame.vStride = pFrame->linesize[2];

                        memcpy(vFrame.yBuf, pFrame->data[0], pCodecCtx->height * vFrame.yStride);
                        memcpy(vFrame.uBuf, pFrame->data[1], (pCodecCtx->height / 2) * vFrame.uStride);
                        memcpy(vFrame.vBuf, pFrame->data[2], (pCodecCtx->height / 2) * vFrame.vStride);

                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameQueue.push_back(vFrame);
                        }
                        
                        if (freeQueue.empty()) break;
                        vFrame = freeQueue.front();
                        freeQueue.pop_front();
                    }
                }
            }
            av_packet_unref(&packet);
            if (vFrame.yBuf) {
                std::lock_guard<std::mutex> lock(frameMutex);
                freeQueue.push_back(vFrame);
            }
        } else {
            av_seek_frame(pFormatCtx, videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(pCodecCtx);
            {
                std::lock_guard<std::mutex> lock(frameMutex);
                while(!frameQueue.empty()){
                    freeQueue.push_back(frameQueue.front());
                    frameQueue.pop_front();
                }
            }
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
    
    SDL_Texture* targetTex = (isVideoMode && videoTexture) ? videoTexture : 
                             (lastDisplayedId != -1 && textures.count(lastDisplayedId)) ? textures[lastDisplayedId] : nullptr;

    if (targetTex) {
        int texW, texH;
        SDL_QueryTexture(targetTex, NULL, NULL, &texW, &texH);
        if (texH > 0) renderW = (int)(512.0f * ((float)texW / (float)texH));
    }
    
    SDL_Rect dst = { dynamicCenterX - (renderW / 2), (Config::SCREEN_HEIGHT / 2) - (renderH / 2), renderW, renderH };

    while (currentEventIndex < bgaEvents.size() && bgaEvents[currentEventIndex].y <= currentPulse) {
        lastDisplayedId = bgaEvents[currentEventIndex].id; currentEventIndex++;
    }
    while (currentLayerIndex < layerEvents.size() && layerEvents[currentLayerIndex].y <= currentPulse) {
        lastLayerId = layerEvents[currentLayerIndex].id; currentLayerIndex++;
    }
    while (currentPoorIndex < poorEvents.size() && poorEvents[currentPoorIndex].y <= currentPulse) {
        lastPoorId = poorEvents[currentPoorIndex].id; currentPoorIndex++;
    }

    if (isVideoMode && videoTexture) {
        double currentTime = sharedVideoElapsed.load(std::memory_order_acquire);
        
        // コンパイルエラー修正: VideoFrameのメンバ順(y,u,v,ys,us,vs,pts)に合わせて初期化
        VideoFrame frameToRender = { nullptr, nullptr, nullptr, 0, 0, 0, -1.0 };

        {
            std::lock_guard<std::mutex> lock(frameMutex);
            while (!frameQueue.empty()) {
                if (frameQueue.size() >= 2 && frameQueue[1].pts <= currentTime) {
                    freeQueue.push_back(frameQueue.front());
                    frameQueue.pop_front();
                    continue;
                }
                if (frameQueue.front().pts <= currentTime + 0.05) {
                    frameToRender = frameQueue.front();
                    frameQueue.pop_front();
                }
                break;
            }
        }

        if (frameToRender.yBuf) {
            void* pixels; int pitch;
            if (SDL_LockTexture(videoTexture, NULL, &pixels, &pitch) == 0) {
                uint8_t* dstY = (uint8_t*)pixels;
                uint8_t* dstU = dstY + (pitch * pCodecCtx->height);
                uint8_t* dstV = dstU + ((pitch / 2) * (pCodecCtx->height / 2));

                for (int i = 0; i < pCodecCtx->height; ++i)
                    memcpy(dstY + i * pitch, frameToRender.yBuf + i * frameToRender.yStride, pCodecCtx->width);
                for (int i = 0; i < pCodecCtx->height / 2; ++i) {
                    memcpy(dstU + i * (pitch / 2), frameToRender.uBuf + i * frameToRender.uStride, pCodecCtx->width / 2);
                    memcpy(dstV + i * (pitch / 2), frameToRender.vBuf + i * frameToRender.vStride, pCodecCtx->width / 2);
                }
                SDL_UnlockTexture(videoTexture);
            }
            std::lock_guard<std::mutex> lock(frameMutex);
            freeQueue.push_back(frameToRender);
        }
        SDL_RenderCopy(renderer, videoTexture, NULL, &dst);
    } else {
        if (lastDisplayedId != -1 && textures.count(lastDisplayedId)) SDL_RenderCopy(renderer, textures[lastDisplayedId], NULL, &dst);
    }

    if (lastLayerId != -1 && textures.count(lastLayerId)) SDL_RenderCopy(renderer, textures[lastLayerId], NULL, &dst);
    if (showPoor && lastPoorId != -1 && textures.count(lastPoorId)) SDL_RenderCopy(renderer, textures[lastPoorId], NULL, &dst);
}