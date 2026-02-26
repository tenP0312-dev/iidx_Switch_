#ifndef SOUNDMANAGER_HPP
#define SOUNDMANAGER_HPP

#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <cstdint> // uint64_t 使用のために追加
#include <vector>

class SoundManager {
public:
    // --- シングルトンインスタンス取得用関数 ---
    static SoundManager& getInstance() {
        static SoundManager instance;
        return instance;
    }

    void init();
    
    // 【修正】boxwav内検索を高速化するための第3引数
    void loadSingleSound(const std::string& filename, const std::string& rootPath, const std::string& bmsonName = "sounds");
    
    // --- 【追加】boxwav内のファイル位置を事前に記録する関数 (コンパイルエラー解消) ---
    void preloadBoxIndex(const std::string& rootPath, const std::string& bmsonName);

    void play(int channel_id);
    void playByName(const std::string& name);
    void clear();
    void stopAll();
    void cleanup();

    // --- 【新規追加】プレビュー再生用関数 ---
    void playPreview(const std::string& fullPath);
    void stopPreview();

    // --- メモリ使用状況取得用Getter ---
    uint64_t getCurrentMemory() const { return currentTotalMemory; }
    uint64_t getMaxMemory() const { return MAX_WAV_MEMORY; }

private:
    // 【修正箇所】警告回避のため、初期化リストの順序をメンバ変数の宣言順に合わせました
    SoundManager() : currentPreviewChunk(nullptr), currentTotalMemory(0) {} 
    ~SoundManager() { cleanup(); }
    SoundManager(const SoundManager&) = delete;            // コピー禁止
    SoundManager& operator=(const SoundManager&) = delete; // 代入禁止

    // --- 【追加】boxwav内のファイル情報を保持する構造体 ---
    struct BoxEntry {
        std::string pckPath;
        uint32_t offset;
        uint32_t size;
    };

    // 音源データ本体
    std::unordered_map<std::string, Mix_Chunk*> sounds;

    // 音源名と再生中チャンネルの紐付け
    std::unordered_map<std::string, int> activeChannels;

    // --- 【追加】boxwavのインデックス情報 (コンパイルエラー解消) ---
    std::unordered_map<std::string, BoxEntry> boxIndex;

    // --- 【新規追加】プレビュー用チャンクの管理 ---
    // ここでの宣言順が初期化順になります
    Mix_Chunk* currentPreviewChunk = nullptr;

    // メモリ管理用
    uint64_t currentTotalMemory = 0;
    const uint64_t MAX_WAV_MEMORY = 512 * 1024 * 1024; // 512MB制限
};

#endif