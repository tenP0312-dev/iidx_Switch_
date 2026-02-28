#ifndef BGAMANAGER_HPP
#define BGAMANAGER_HPP

#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include "CommonTypes.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

struct BgaEvent {
    long long y;
    int id;
};

class BgaManager {
public:
    BgaManager() = default;
    ~BgaManager() { cleanup(); }

    void init(size_t expectedSize = 512);
    void setBgaDirectory(const std::string& dir) { baseDir = dir; }
    void registerPath(int id, const std::string& filename) { idToFilename[id] = filename; }
    void loadBmp(int id, const std::string& fullPath, SDL_Renderer* renderer);
    bool loadBgaFile(const std::string& path, SDL_Renderer* renderer);
    void preLoad(long long startPulse, SDL_Renderer* renderer);
    void setEvents(const std::vector<BgaEvent>& events) { bgaEvents = events; currentEventIndex = 0; }
    void setLayerEvents(const std::vector<BgaEvent>& events) { layerEvents = events; currentLayerIndex = 0; }
    void setPoorEvents(const std::vector<BgaEvent>& events) { poorEvents = events; currentPoorIndex = 0; }
    void syncTime(double ms);
    void render(long long currentPulse, SDL_Renderer* renderer, int x, int y, double cur_ms = 0.0);
    void setMissTrigger(bool active) { showPoor = active; }
    void clear();
    void cleanup();

private:
    void videoWorker();

    // ★修正：テクスチャとサイズを一緒に管理し、render() での SDL_QueryTexture を廃止する
    struct BgaTextureEntry {
        SDL_Texture* tex = nullptr;
        int w = 0;
        int h = 0;
    };

    std::unordered_map<int, BgaTextureEntry> textures;  // SDL_Texture* → BgaTextureEntry に変更
    std::unordered_map<int, std::string>     idToFilename;
    std::string baseDir;
    std::vector<BgaEvent> bgaEvents, layerEvents, poorEvents;
    size_t currentEventIndex = 0, currentLayerIndex = 0, currentPoorIndex = 0;
    int lastDisplayedId = -1, lastLayerId = -1, lastPoorId = -1;
    bool showPoor = false;

    bool isVideoMode         = false;
    bool hasNewFrameToUpload = false;

    // 動画テクスチャのサイズも事前に保持する
    SDL_Texture* videoTexture = nullptr;
    int videoTexW = 0;
    int videoTexH = 0;

    AVFormatContext* pFormatCtx = nullptr;
    AVCodecContext*  pCodecCtx  = nullptr;
    AVFrame*         pFrame     = nullptr;
    int              videoStreamIdx = -1;

    std::thread             decodeThread;
    std::mutex              frameMutex;
    std::atomic<bool>       quitThread{false};
    std::atomic<double>     sharedVideoElapsed{0.0};

    std::deque<VideoFrame>  frameQueue;
    const size_t MAX_FRAME_QUEUE = 30;

    // メモリ断片化防止用固定プール
    std::vector<uint8_t> memoryPoolV;
};

#endif // BGAMANAGER_HPP




