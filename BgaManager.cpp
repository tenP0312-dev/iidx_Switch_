#include "BgaManager.hpp"
#include "Config.hpp" // レーン幅等を参照するために追加
#include <SDL2/SDL_image.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <map>
#include <unordered_map>
#include <cstring>
#include <thread>

// --- 定数の定義（モードにより使い分け） ---
static const long long BMP_LOOK_AHEAD = 300000;
static const long long BMP_KEEP_ALIVE = 300000;
static const int BMP_LOAD_INTERVAL = 1;

static const long long VIDEO_LOOK_AHEAD = 5000;
static const long long VIDEO_KEEP_ALIVE = 5000;
static const int VIDEO_LOAD_INTERVAL = 6;

void BgaManager::init(size_t expectedSize) {
    // 物理メモリの強制解放
    std::unordered_map<int, SDL_Texture*>().swap(textures);
    textures.reserve(std::min((size_t)256, expectedSize));
    
    bgaEvents.clear();
    layerEvents.clear();
    poorEvents.clear();
    
    idToFilename.clear();
    lastUsedPulse.clear();
    
    currentEventIndex = 0;
    currentLayerIndex = 0;
    currentPoorIndex = 0;
    
    lastDisplayedId = -1;
    lastLayerId = -1;
    lastPoorId = -1;
    showPoor = false;

    // --- 動画系初期化 ---
    isVideoMode = false;
    videoTexture = nullptr;
    std::vector<uint8_t>().swap(videoData);
    std::vector<uint8_t>().swap(nextVideoData);
    std::vector<size_t>().swap(frameOffsets);
    std::vector<size_t>().swap(nextFrameOffsets);
    
    videoTotalFrames = 0;
    lastFrameIndex = -1;
    videoStartMs = -1.0;
    currentPartIndex = 1;
    baseVideoPath = "";
    totalFramesBeforeCurrentPart = 0;
    
    isLoading = false;
    hasNextPartReady = false;
    if (loaderThread.joinable()) loaderThread.join();

    std::map<int, SDL_Texture*>().swap(videoCache);

    std::cout << "[BGA] Hybrid Engine Init (BMP Stability + Video Support)." << std::endl;
}

void BgaManager::asyncLoadWorker(int partIdx) {
    std::string path = baseVideoPath;
    if (partIdx > 1) path += std::to_string(partIdx);

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        isLoading = false;
        return;
    }

    std::vector<uint8_t> tempData;
    std::vector<size_t> tempOffsets;
    uint32_t tempTotal;

    ifs.read(reinterpret_cast<char*>(&tempTotal), 4);
    uint32_t dummyW, dummyH;
    ifs.read(reinterpret_cast<char*>(&dummyW), 4);
    ifs.read(reinterpret_cast<char*>(&dummyH), 4);

    ifs.seekg(0, std::ios::end);
    size_t fileSize = ifs.tellg();
    ifs.seekg(12, std::ios::beg);

    tempData.resize(fileSize - 12);
    ifs.read(reinterpret_cast<char*>(tempData.data()), tempData.size());

    size_t currentPos = 0;
    for (uint32_t i = 0; i < tempTotal; ++i) {
        if (currentPos + 4 > tempData.size()) break;
        tempOffsets.push_back(currentPos);
        uint32_t fSize;
        memcpy(&fSize, &tempData[currentPos], 4);
        currentPos += 4 + fSize;
    }

    nextVideoData = std::move(tempData);
    nextFrameOffsets = std::move(tempOffsets);
    nextPartTotalFrames = tempTotal;

    hasNextPartReady = true;
    isLoading = false;
}

bool BgaManager::loadPart(int partIdx, SDL_Renderer* renderer) {
    std::string path = baseVideoPath;
    if (partIdx > 1) path += std::to_string(partIdx);

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    for (auto& pair : videoCache) if (pair.second) SDL_DestroyTexture(pair.second);
    std::map<int, SDL_Texture*>().swap(videoCache);
    
    std::vector<uint8_t>().swap(videoData);
    std::vector<size_t>().swap(frameOffsets);

    ifs.read(reinterpret_cast<char*>(&videoTotalFrames), 4);
    ifs.read(reinterpret_cast<char*>(&videoW), 4);
    ifs.read(reinterpret_cast<char*>(&videoH), 4);

    ifs.seekg(0, std::ios::end);
    size_t fileSize = ifs.tellg();
    ifs.seekg(12, std::ios::beg);

    videoData.resize(fileSize - 12);
    ifs.read(reinterpret_cast<char*>(videoData.data()), videoData.size());

    size_t currentPos = 0;
    for (uint32_t i = 0; i < videoTotalFrames; ++i) {
        if (currentPos + 4 > videoData.size()) break;
        frameOffsets.push_back(currentPos);
        uint32_t fSize;
        memcpy(&fSize, &videoData[currentPos], 4);
        currentPos += 4 + fSize;
    }
    return true;
}

bool BgaManager::loadBgaFile(const std::string& path, SDL_Renderer* renderer) {
    std::string targetPath = path;
    static const std::vector<std::string> videoExts = {
        ".mp4", ".avi", ".wmv", ".mov", ".mkv", ".mpg", ".mpeg", ".flv", ".webm"
    };
    std::string lowerPath = targetPath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    for (const auto& ext : videoExts) {
        size_t pos = lowerPath.rfind(ext);
        if (pos != std::string::npos && pos == lowerPath.length() - ext.length()) {
            targetPath.replace(pos, ext.length(), ".bga");
            break;
        }
    }

    baseVideoPath = targetPath;
    currentPartIndex = 1;
    totalFramesBeforeCurrentPart = 0;

    if (loadPart(currentPartIndex, renderer)) {
        isVideoMode = true;
        // 初回フレームプリロード
        if (!frameOffsets.empty()) {
            size_t offset = frameOffsets[0];
            uint32_t fSize;
            memcpy(&fSize, &videoData[offset], 4);
            SDL_RWops* rw = SDL_RWFromMem(&videoData[offset + 4], fSize);
            if (rw) {
                SDL_Texture* tex = IMG_LoadTexture_RW(renderer, rw, 1);
                if (tex) videoCache[0] = tex;
            }
        }
        return true;
    }
    return false;
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

    if (!scan(bgaEvents)) {
        if (!scan(layerEvents)) scan(poorEvents);
    }

    if (nextNeededId != -1 && idToFilename.count(nextNeededId)) {
        loadBmp(nextNeededId, baseDir + idToFilename[nextNeededId], renderer);
    }
}

void BgaManager::loadBmp(int id, const std::string& fullPath, SDL_Renderer* renderer) {
    if (textures.count(id)) return;

    // 動画モードでない場合、安定版の挙動を維持（無闇に削除しない）
    if (!isVideoMode && textures.size() > 256) {
        auto it = textures.begin();
        if (it->second) SDL_DestroyTexture(it->second);
        lastUsedPulse.erase(it->first);
        textures.erase(it);
    }

    SDL_Surface* surf = IMG_Load(fullPath.c_str());
    if (!surf) surf = SDL_LoadBMP(fullPath.c_str());
    if (!surf) return;

    SDL_Surface* optimized = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surf);
    if (!optimized) return;

    SDL_SetColorKey(optimized, SDL_TRUE, SDL_MapRGB(optimized->format, 0, 0, 0));
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, optimized);
    SDL_FreeSurface(optimized);

    if (tex) {
        textures[id] = tex;
        lastUsedPulse[id] = 0;
    }
}

void BgaManager::render(long long currentPulse, SDL_Renderer* renderer, int x, int y, double cur_ms) {
    // --- 修正箇所: レーン幅とサイドに基づいた動的な位置計算 ---
    int sw = Config::SCRATCH_WIDTH;
    int lw = Config::LANE_WIDTH;
    int totalWidth = (7 * lw) + sw + 10;
    int startX = (Config::PLAY_SIDE == 1) ? 50 : (Config::SCREEN_WIDTH - totalWidth - 50);

    int dynamicCenterX;
    if (Config::PLAY_SIDE == 1) {
        int laneRightEdge = startX + totalWidth;
        dynamicCenterX = laneRightEdge + (Config::SCREEN_WIDTH - laneRightEdge) / 2;
    } else {
        dynamicCenterX = startX / 2;
    }

    // 引数xに依存せず、計算した中心座標からBGA(512x512)を配置
    SDL_Rect dst = { dynamicCenterX - 256, y, 512, 512 };
    // -------------------------------------------------------

    bool videoActuallyDrawn = false;

    // --- 【イベント更新 セクション】 (順序修正：動画描画の前に更新を行いy=0に対応) ---
    while (currentEventIndex < bgaEvents.size() && bgaEvents[currentEventIndex].y <= currentPulse) {
        lastDisplayedId = bgaEvents[currentEventIndex].id;
        if (lastDisplayedId != -1) lastUsedPulse[lastDisplayedId] = currentPulse;
        if (isVideoMode && videoStartMs < 0) videoStartMs = cur_ms;
        currentEventIndex++;
    }
    while (currentLayerIndex < layerEvents.size() && layerEvents[currentLayerIndex].y <= currentPulse) {
        lastLayerId = layerEvents[currentLayerIndex].id;
        if (lastLayerId != -1) lastUsedPulse[lastLayerId] = currentPulse;
        currentLayerIndex++;
    }
    while (currentPoorIndex < poorEvents.size() && poorEvents[currentPoorIndex].y <= currentPulse) {
        lastPoorId = poorEvents[currentPoorIndex].id;
        if (lastPoorId != -1) lastUsedPulse[lastPoorId] = currentPulse;
        currentPoorIndex++;
    }

    // --- 【動画ロジック セクション】 ---
    if (isVideoMode && !videoData.empty()) {
        double videoElapsed = (videoStartMs >= 0) ? cur_ms - videoStartMs : 0.0;
        int globalFrameIndex = (int)(videoElapsed * 15.0 / 1000.0);
        int localFrameIndex = globalFrameIndex - totalFramesBeforeCurrentPart;

        // パート切り替え
        if (hasNextPartReady && localFrameIndex >= (int)videoTotalFrames) {
            SDL_Texture* firstTex = nullptr;
            if (!nextFrameOffsets.empty()) {
                size_t offset = nextFrameOffsets[0];
                uint32_t fSize;
                memcpy(&fSize, &nextVideoData[offset], 4);
                SDL_RWops* rw = SDL_RWFromMem(&nextVideoData[offset + 4], fSize);
                if (rw) firstTex = IMG_LoadTexture_RW(renderer, rw, 1);
            }
            videoData.swap(nextVideoData);
            frameOffsets.swap(nextFrameOffsets);
            std::vector<uint8_t>().swap(nextVideoData);
            std::vector<size_t>().swap(nextFrameOffsets);
            totalFramesBeforeCurrentPart += videoTotalFrames;
            videoTotalFrames = nextPartTotalFrames;
            currentPartIndex++;
            for (auto& pair : videoCache) if (pair.second) SDL_DestroyTexture(pair.second);
            std::map<int, SDL_Texture*>().swap(videoCache);
            if (firstTex) videoCache[0] = firstTex;
            hasNextPartReady = false;
            localFrameIndex = globalFrameIndex - totalFramesBeforeCurrentPart;
        }

        // 描画と先行デコード
        int drawFrameIdx = std::max(0, std::min(localFrameIndex, (int)frameOffsets.size() - 1));
        if (!frameOffsets.empty()) {
            // 非同期ロード開始判断
            if (localFrameIndex >= (int)videoTotalFrames - 150 && !isLoading && !hasNextPartReady) {
                isLoading = true;
                if (loaderThread.joinable()) loaderThread.join();
                loaderThread = std::thread(&BgaManager::asyncLoadWorker, this, currentPartIndex + 1);
            }
            // 次フレーム予約
            int nextIdx = drawFrameIdx + 1;
            if (nextIdx < (int)frameOffsets.size() && videoCache.find(nextIdx) == videoCache.end()) {
                size_t offset = frameOffsets[nextIdx];
                uint32_t fSize;
                memcpy(&fSize, &videoData[offset], 4);
                SDL_RWops* rw = SDL_RWFromMem(&videoData[offset + 4], fSize);
                if (rw) {
                    SDL_Texture* tex = IMG_LoadTexture_RW(renderer, rw, 1);
                    if (tex) videoCache[nextIdx] = tex;
                }
            }
            // 描画
            auto it = videoCache.find(drawFrameIdx);
            if (it != videoCache.end()) {
                SDL_RenderCopy(renderer, it->second, NULL, &dst);
                videoActuallyDrawn = true;
            }
            // キャッシュ掃除
            if (drawFrameIdx != lastFrameIndex) {
                auto itDel = videoCache.begin();
                while (itDel != videoCache.end() && itDel->first < drawFrameIdx) {
                    if (itDel->second) SDL_DestroyTexture(itDel->second);
                    itDel = videoCache.erase(itDel);
                }
                lastFrameIndex = drawFrameIdx;
            }
        }
    }

    // --- 【BMP動動的ロード セクション】 (優先度付き2段構え修正) ---
    static int frameThrottle = 0;
    int currentInterval = isVideoMode ? VIDEO_LOAD_INTERVAL : BMP_LOAD_INTERVAL;

    if (++frameThrottle >= currentInterval) {
        frameThrottle = 0;
        int nextId = -1;

        // スキャン用の共通関数
        auto scanSpecificRange = [&](long long lookAheadMs) {
            auto scanList = [&](const std::vector<BgaEvent>& events, size_t startIndex) {
                for (size_t i = startIndex; i < events.size(); ++i) {
                    if (events[i].y > currentPulse + lookAheadMs) break;
                    if (textures.find(events[i].id) == textures.end()) {
                        nextId = events[i].id;
                        return true;
                    }
                }
                return false;
            };
            if (scanList(bgaEvents, currentEventIndex)) return true;
            if (scanList(layerEvents, currentLayerIndex)) return true;
            if (scanList(poorEvents, currentPoorIndex)) return true;
            return false;
        };

        // 第1優先：直近 5秒間 (5000ms) を最優先でチェック
        bool urgentFound = scanSpecificRange(5000);

        // 第2優先：緊急がなければ本来の LookAhead をチェック
        if (!urgentFound) {
            scanSpecificRange(isVideoMode ? VIDEO_LOOK_AHEAD : BMP_LOOK_AHEAD);
        }

        if (nextId != -1 && idToFilename.count(nextId)) {
            loadBmp(nextId, baseDir + idToFilename[nextId], renderer);
        }
    }

    // --- 【クリーンアップ セクション】 ---
    static int cleanupCounter = 0;
    if (++cleanupCounter > 180) {
        cleanupCounter = 0;
        long long currentKeepAlive = isVideoMode ? VIDEO_KEEP_ALIVE : BMP_KEEP_ALIVE;
        for (auto it = textures.begin(); it != textures.end(); ) {
            int id = it->first;
            if (id != lastDisplayedId && id != lastLayerId && id != lastPoorId) {
                if (std::abs(lastUsedPulse[id] - currentPulse) > currentKeepAlive) {
                    if (it->second) SDL_DestroyTexture(it->second);
                    lastUsedPulse.erase(id);
                    it = textures.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    // --- 【最終描画 セクション】 ---
    if (!videoActuallyDrawn && lastDisplayedId != -1 && textures.count(lastDisplayedId)) {
        SDL_RenderCopy(renderer, textures[lastDisplayedId], NULL, &dst);
        lastUsedPulse[lastDisplayedId] = currentPulse;
    }
    if (lastLayerId != -1 && textures.count(lastLayerId)) {
        SDL_RenderCopy(renderer, textures[lastLayerId], NULL, &dst);
        lastUsedPulse[lastLayerId] = currentPulse;
    }
    if (showPoor && lastPoorId != -1 && textures.count(lastPoorId)) {
        SDL_RenderCopy(renderer, textures[lastPoorId], NULL, &dst);
        lastUsedPulse[lastPoorId] = currentPulse;
    }
}

void BgaManager::clear() {
    if (loaderThread.joinable()) loaderThread.join();
    isLoading = false;
    hasNextPartReady = false;

    for (auto& pair : textures) if (pair.second) SDL_DestroyTexture(pair.second);
    std::unordered_map<int, SDL_Texture*>().swap(textures);

    for (auto& pair : videoCache) if (pair.second) SDL_DestroyTexture(pair.second);
    std::map<int, SDL_Texture*>().swap(videoCache);
    
    if (videoTexture) { SDL_DestroyTexture(videoTexture); videoTexture = nullptr; }
    
    std::vector<uint8_t>().swap(videoData);
    std::vector<uint8_t>().swap(nextVideoData);
    std::vector<size_t>().swap(frameOffsets);
    std::vector<size_t>().swap(nextFrameOffsets);
    
    bgaEvents.clear(); layerEvents.clear(); poorEvents.clear();
    currentEventIndex = 0; currentLayerIndex = 0; currentPoorIndex = 0;
    lastDisplayedId = -1; lastLayerId = -1; lastPoorId = -1;
    showPoor = false; isVideoMode = false;
    videoStartMs = -1.0; currentPartIndex = 1; baseVideoPath = "";
    totalFramesBeforeCurrentPart = 0;
}

void BgaManager::cleanup() { clear(); }