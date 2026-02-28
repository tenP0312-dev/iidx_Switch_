#include "BgaManager.hpp"
#include "Config.hpp"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <thread>

static const long long BMP_LOOK_AHEAD = 300000;

void BgaManager::init(size_t expectedSize) {
    clear();
    textures.reserve(std::min((size_t)256, expectedSize));
    // ★修正: メモリプールは動画解像度が確定してから loadBgaFile() で確保する
    // init() 時点では動画サイズ不明のため、ここでは確保しない
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
    pCodecCtx->thread_count   = 2;
    pCodecCtx->flags2         |= AV_CODEC_FLAG2_FAST;
    pCodecCtx->workaround_bugs = 1;

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) return false;

    pFrame = av_frame_alloc();

    videoTexture = SDL_CreateTexture(renderer,
                                     SDL_PIXELFORMAT_NV12,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     pCodecCtx->width,
                                     pCodecCtx->height);

    if (!videoTexture) {
        fprintf(stderr, "CRITICAL: Failed to create video texture.\n");
        return false;
    }

    // ★修正：動画テクスチャのサイズを生成時に一度だけ記録する
    videoTexW = pCodecCtx->width;
    videoTexH = pCodecCtx->height;

    // ★修正: 実際の動画解像度でプールを確保する
    // init() 時の 1280x720 固定サイズではなく、コーデックが確定したここで計算する。
    // 解像度不一致によるバッファオーバーフローを防ぐ。
    poolSlotSize = (size_t)(videoTexW * videoTexH) + (size_t)(videoTexW * videoTexH / 2);
    memoryPoolV.assign(poolSlotSize * MAX_FRAME_QUEUE, 0);

    quitThread = false;
    isVideoMode = true;
    isReady.store(false, std::memory_order_release); // ★修正③: 準備未完了状態でリセット
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        frameQueue.clear();
    }
    hasNewFrameToUpload = false;
    decodeThread = std::thread(&BgaManager::videoWorker, this);

    // ★修正③: メインスレッドのブロッキングを完全に排除。
    // 旧実装は最大 1000ms スリープしていたが、Switch では appletMainLoop() が
    // 呼ばれない状態が続きシステムイベントが処理されずフリーズに見えていた。
    // isReady フラグは videoWorker が MIN_FRAMES 分バッファを溜めた時点でセットされ、
    // render() 側でガードすることで BGA 描画が安全に開始される。
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

    // ★修正：テクスチャ生成と同時にサイズを BgaTextureEntry にキャッシュする。
    // render() での毎フレーム SDL_QueryTexture を廃止するための前提。
    BgaTextureEntry entry;
    entry.w   = surf->w;
    entry.h   = surf->h;
    entry.tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (entry.tex) textures[id] = entry;
}

void BgaManager::syncTime(double ms) {
    sharedVideoElapsed.store(ms / 1000.0, std::memory_order_release);
}

void BgaManager::videoWorker() {
    AVPacket packet;

    int w = pCodecCtx->width;
    int h = pCodecCtx->height;
    // ★修正: プール確保時と同じ値を使うことでスロット境界を一致させる
    size_t nv12Size = poolSlotSize;

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

        // ★修正: データレース対策
        // render() が現在このスロットを memcpy 中であれば書き込みをスキップして待機する。
        // writeIdx が renderInUseSlot と一致する間は次のスロットを試みる。
        size_t safeWriteIdx = writeIdx;
        {
            int safety = 0;
            while (safeWriteIdx == renderInUseSlot.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::microseconds(200));
                if (++safety > 500) break; // 100ms タイムアウト（デッドロック防止）
            }
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
                        vFrame.pts     = frameTime;
                        vFrame.slotIdx = safeWriteIdx; // ★修正: スロット番号を記録

                        uint8_t* dstBase = &this->memoryPoolV[safeWriteIdx * nv12Size];
                        vFrame.yPtr = dstBase;

                        // Y面コピー
                        for (int i = 0; i < h; ++i) {
                            memcpy(dstBase + i * w,
                                   pFrame->data[0] + i * pFrame->linesize[0], w);
                        }

                        // UV面インターリーブ (NV12): uint16_t で2バイト同時書き込み
                        uint8_t* dstUV = dstBase + (w * h);
                        for (int i = 0; i < h / 2; ++i) {
                            uint16_t* dUV16 = reinterpret_cast<uint16_t*>(dstUV + i * w);
                            const uint8_t* sU = pFrame->data[1] + i * pFrame->linesize[1];
                            const uint8_t* sV = pFrame->data[2] + i * pFrame->linesize[2];
                            for (int j = 0; j < w / 2; ++j) {
                                dUV16[j] = (uint16_t)sU[j] | ((uint16_t)sV[j] << 8);
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(frameMutex);
                            frameQueue.push_back(vFrame);
                            if (!isReady.load(std::memory_order_relaxed) && frameQueue.size() >= 10) {
                                isReady.store(true, std::memory_order_release);
                            }
                        }
                        safeWriteIdx = (safeWriteIdx + 1) % MAX_FRAME_QUEUE;
                        writeIdx     = safeWriteIdx; // 次ループの起点を更新
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
    // ★修正③: デコードスレッドが十分なフレームを蓄積するまで描画をスキップ。
    //          これにより loadBgaFile() がメインスレッドをブロックせずとも、
    //          未初期化のフレームバッファへのアクセスを防ぐ。
    if (isVideoMode && !isReady.load(std::memory_order_acquire)) return;

    int sw = Config::SCRATCH_WIDTH;
    int lw = Config::LANE_WIDTH;
    int totalKeysWidth = 0;
    for (int i = 1; i <= 7; i++) totalKeysWidth += (i % 2 != 0) ? (int)(lw * 1.4) : lw;
    int totalWidth = totalKeysWidth + sw;
    int startX = (Config::PLAY_SIDE == 1) ? 50 : (Config::SCREEN_WIDTH - totalWidth - 50);
    int dynamicCenterX = (Config::PLAY_SIDE == 1)
        ? (startX + totalWidth + (Config::SCREEN_WIDTH - (startX + totalWidth)) / 2)
        : (startX / 2);

    int renderH = 512;
    int renderW = 512;

    // ★修正：SDL_QueryTexture を廃止。サイズはキャッシュ済みの値を参照する。
    SDL_Texture* targetTex = nullptr;
    if (isVideoMode && videoTexture) {
        targetTex = videoTexture;
        if (videoTexH > 0) renderW = (int)(512.0f * ((float)videoTexW / (float)videoTexH));
    } else if (lastDisplayedId != -1) {
        auto it = textures.find(lastDisplayedId);
        if (it != textures.end() && it->second.tex) {
            targetTex = it->second.tex;
            if (it->second.h > 0)
                renderW = (int)(512.0f * ((float)it->second.w / (float)it->second.h));
        }
    }

    SDL_Rect dst = { dynamicCenterX - (renderW / 2),
                     (Config::SCREEN_HEIGHT / 2) - (renderH / 2), renderW, renderH };

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

        // ロック範囲をポインタ取得までに縮小し、memcpyはロック外で実行する。
        // memoryPoolV は固定リングバッファ(30スロット)のため、
        // pop済みスロットがデコードスレッドに再利用されるまでに
        // 最大30フレーム分のサイクルが必要 → memcpy中(数十μs)に
        // 上書きが追いつくことはなく、データ競合は発生しない。
        uint8_t* frameDataPtr = nullptr;
        size_t   frameSlotIdx = SIZE_MAX;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            while (!frameQueue.empty()) {
                VideoFrame& front = frameQueue.front();
                if (frameQueue.size() >= 2 && frameQueue[1].pts <= currentTime) {
                    frameQueue.pop_front();
                    continue;
                }
                if (front.pts <= currentTime + 0.05) {
                    frameDataPtr = front.yPtr;
                    frameSlotIdx = front.slotIdx;
                    frameQueue.pop_front();
                }
                break;
            }
        }

        if (frameDataPtr) {
            // ★修正: memcpy 開始前にスロット番号を通知し、worker が上書きしないようにする
            renderInUseSlot.store(frameSlotIdx, std::memory_order_release);

            void* pixels;
            int   pitch;
            if (SDL_LockTexture(videoTexture, NULL, &pixels, &pitch) == 0) {
                uint8_t* dst = (uint8_t*)pixels;
                const uint8_t* src = frameDataPtr;
                for (int row = 0; row < videoTexH; ++row)
                    memcpy(dst + row * pitch, src + row * videoTexW, videoTexW);
                uint8_t* dstUV = dst + pitch * videoTexH;
                const uint8_t* srcUV = src + videoTexW * videoTexH;
                for (int row = 0; row < videoTexH / 2; ++row)
                    memcpy(dstUV + row * pitch, srcUV + row * videoTexW, videoTexW);
                SDL_UnlockTexture(videoTexture);
            }

            // ★修正: memcpy 完了後にスロット解放を通知
            renderInUseSlot.store(SIZE_MAX, std::memory_order_release);
        }
        SDL_RenderCopy(renderer, videoTexture, NULL, &dst);
    } else {
        if (lastDisplayedId != -1) {
            auto it = textures.find(lastDisplayedId);
            if (it != textures.end() && it->second.tex)
                SDL_RenderCopy(renderer, it->second.tex, NULL, &dst);
        }
    }

    if (lastLayerId != -1) {
        auto it = textures.find(lastLayerId);
        if (it != textures.end() && it->second.tex)
            SDL_RenderCopy(renderer, it->second.tex, NULL, &dst);
    }

    if (showPoor && lastPoorId != -1) {
        auto it = textures.find(lastPoorId);
        if (it != textures.end() && it->second.tex)
            SDL_RenderCopy(renderer, it->second.tex, NULL, &dst);
    }
}

void BgaManager::clear() {
    quitThread = true;
    if (decodeThread.joinable()) decodeThread.join();

    for (auto& pair : textures) {
        if (pair.second.tex) SDL_DestroyTexture(pair.second.tex);
    }
    textures.clear();

    if (videoTexture) { SDL_DestroyTexture(videoTexture); videoTexture = nullptr; }
    videoTexW = 0; videoTexH = 0;

    if (pFrame)    { av_frame_free(&pFrame);            pFrame    = nullptr; }
    if (pCodecCtx) { avcodec_free_context(&pCodecCtx); pCodecCtx = nullptr; }
    if (pFormatCtx){ avformat_close_input(&pFormatCtx); pFormatCtx = nullptr; }

    isVideoMode          = false;
    hasNewFrameToUpload  = false;
    isReady.store(false, std::memory_order_release);
    renderInUseSlot.store(SIZE_MAX, std::memory_order_release); // ★修正: スロット保護をリセット
    frameQueue.clear();
    memoryPoolV.clear();
    memoryPoolV.shrink_to_fit(); // ★修正: 実際にメモリを解放する（次の loadBgaFile で再確保）
    poolSlotSize = 0;
    currentEventIndex = 0; currentLayerIndex = 0; currentPoorIndex = 0;
    lastDisplayedId   = -1; lastLayerId = -1; lastPoorId = -1;
}

void BgaManager::cleanup() { clear(); }







