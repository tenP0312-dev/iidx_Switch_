#ifndef BGAMANAGER_HPP
#define BGAMANAGER_HPP

#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque> // 追加: バッファ用

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

// --- 追加: デコード済みフレームを保持する構造体 ---
struct VideoFrame {
    std::vector<uint8_t> yBuf;
    std::vector<uint8_t> uBuf;
    std::vector<uint8_t> vBuf;
    int yStride, uStride, vStride;
    double pts; // 表示予定時刻
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

    std::unordered_map<int, SDL_Texture*> textures;
    std::unordered_map<int, std::string> idToFilename;
    std::string baseDir;

    std::vector<BgaEvent> bgaEvents;
    std::vector<BgaEvent> layerEvents;
    std::vector<BgaEvent> poorEvents;
    size_t currentEventIndex = 0;
    size_t currentLayerIndex = 0;
    size_t currentPoorIndex = 0;
    int lastDisplayedId = -1;
    int lastLayerId = -1;
    int lastPoorId = -1;
    bool showPoor = false;

    // --- 動画再生エンジン ---
    bool isVideoMode = false;
    SDL_Texture* videoTexture = nullptr;
    AVFormatContext* pFormatCtx = nullptr;
    AVCodecContext* pCodecCtx = nullptr;
    AVFrame* pFrame = nullptr;
    int videoStreamIdx = -1;
    
    std::thread decodeThread;
    std::mutex frameMutex;
    std::atomic<bool> quitThread{false};
    std::atomic<double> sharedVideoElapsed{0.0};
    
    // --- 追加: フレームバッファリング用 ---
    std::deque<VideoFrame> frameQueue; // リングバッファ代わりのDeque
    const size_t MAX_FRAME_QUEUE = 60; // 最大10フレーム先行バッファ
    
    // 描画用の一時保持データ（テクスチャ更新用）
    std::vector<uint8_t> currentY, currentU, currentV;
    int currentYStride, currentUStride, currentVStride;
    bool hasNewFrameToUpload = false;
};

#endif