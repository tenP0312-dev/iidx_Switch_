#ifndef BGAMANAGER_HPP
#define BGAMANAGER_HPP

#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <map> 
#include <vector>
#include <cstdint>
#include <thread> // ★追加：非同期ロード用
#include <atomic> // ★追加：スレッド安全なフラグ用

// bga_events の1行分を保持する構造体 (100% Logic Preservation)
struct BgaEvent {
    long long y; // 発動タイミング（Pulse単位）
    int id;      // 表示する画像のID
};

class BgaManager {
public:
    BgaManager() = default;
    ~BgaManager() { cleanup(); }

    // 初期化とメモリ確保
    void init(size_t expectedSize = 512);

    // --- 【追加：動的キャッシュ用管理関数】 ---
    void setBgaDirectory(const std::string& dir) { baseDir = dir; }
    void registerPath(int id, const std::string& filename) { idToFilename[id] = filename; }

    // 画像のロード（静止画BGA用）
    void loadBmp(int id, const std::string& fullPath, SDL_Renderer* renderer);

    // --- 【追加：動画BGA用ロード関数】 ---
    bool loadBgaFile(const std::string& path, SDL_Renderer* renderer);

    // ★追加：Ready時間中に先行して読み込むための関数宣言
    void preLoad(long long startPulse, SDL_Renderer* renderer);

    // --- 既存のsetEventsを継承 ---
    void setEvents(const std::vector<BgaEvent>& events) {
        bgaEvents = events;
        currentEventIndex = 0;
        lastDisplayedId = -1;
    }

    void setLayerEvents(const std::vector<BgaEvent>& events) {
        layerEvents = events;
        currentLayerIndex = 0;
        lastLayerId = -1;
    }

    void setPoorEvents(const std::vector<BgaEvent>& events) {
        poorEvents = events;
        currentPoorIndex = 0;
        lastPoorId = -1;
    }

    // 現在のPulse値に基づいた描画 (引数に cur_ms を追加)
    void render(long long currentPulse, SDL_Renderer* renderer, int x, int y, double cur_ms = 0.0);

    // ミス状態のフラグ
    void setMissTrigger(bool active) { showPoor = active; }

    // シーン終了時にテクスチャを解放
    void clear();
    void cleanup();

private:
    // --- 【追加：内部処理用（分割読み込み）】 ---
    bool loadPart(int partIdx, SDL_Renderer* renderer);

    // ★追加：非同期ロード用ワーカースレッド関数
    void asyncLoadWorker(int partIdx);

    // --- 【既存メンバ：静止画BGA用（100%継承）】 ---
    std::unordered_map<int, SDL_Texture*> textures;
    std::unordered_map<int, std::string> idToFilename;
    std::unordered_map<int, long long> lastUsedPulse;
    std::string baseDir;

    std::vector<BgaEvent> bgaEvents;
    size_t currentEventIndex = 0;
    int lastDisplayedId = -1;

    std::vector<BgaEvent> layerEvents;
    size_t currentLayerIndex = 0;
    int lastLayerId = -1;

    std::vector<BgaEvent> poorEvents;
    size_t currentPoorIndex = 0;
    int lastPoorId = -1;
    bool showPoor = false;

    // --- 【追加メンバ：動画専用ロジック用（分離処理用）】 ---
    bool isVideoMode = false;          // 動画再生中かどうかのフラグ
    SDL_Texture* videoTexture = nullptr; // 動画描画用のストリーミングテクスチャ
    std::vector<uint8_t> videoData;   // 全フレームのRaw RGBAデータ
    uint32_t videoTotalFrames = 0;    // 総フレーム数
    uint32_t videoW = 0;              // 動画の幅 (256想定)
    uint32_t videoH = 0;              // 動画の高さ (256想定)
    int lastFrameIndex = -1;          // 最後に更新したフレーム番号

    // --- ★追加：動画データ内の各フレーム位置を保持（高速化・エラー解消用） ---
    std::vector<size_t> frameOffsets; 

    // --- ★追加：先読み安定化用キャッシュ ---
    std::map<int, SDL_Texture*> videoCache; // フレーム番号をキーにしたテクスチャキャッシュ

    // --- ★追加：動画再生の基準時刻（同期位置修正用） ---
    double videoStartMs = -1.0; 

    // --- ★追加：分割管理メンバ ---
    int currentPartIndex = 1;                // 現在のパート番号
    std::string baseVideoPath;               // 元の.bgaファイルのフルパス保持
    int totalFramesBeforeCurrentPart = 0;    // 現在のパートまでに経過した累積フレーム数

    // --- ★追加：非同期ロード（運び屋スレッド）用メンバ ---
    std::thread loaderThread;                 // ロード用スレッド
    std::atomic<bool> isLoading{false};       // スレッドが動いているかどうかのフラグ
    std::atomic<bool> hasNextPartReady{false}; // 次のパートの準備が完了したか
    std::vector<uint8_t> nextVideoData;       // 予備バッファ（裏でここに読み込む）
    std::vector<size_t> nextFrameOffsets;     // 予備インデックス
    uint32_t nextPartTotalFrames = 0;         // 次のパートのフレーム数
};

#endif