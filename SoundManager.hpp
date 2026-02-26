#ifndef SOUNDMANAGER_HPP
#define SOUNDMANAGER_HPP

#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <cstdint> 
#include <vector>
#include <functional>

class SoundManager {
public:
    static SoundManager& getInstance() {
        static SoundManager instance;
        return instance;
    }

    void init();
    
    void loadSingleSound(const std::string& filename, const std::string& rootPath, const std::string& bmsonName = "sounds");
    
    void loadSoundsInBulk(const std::vector<std::string>& filenames, 
                          const std::string& rootPath, 
                          const std::string& bmsonName,
                          std::function<void(int, const std::string&)> onProgress = nullptr);

    void preloadBoxIndex(const std::string& rootPath, const std::string& bmsonName);

    // --- 既存ロジック100%継承: 数値IDによる再生 ---
    void play(int soundId); 
    void playByName(const std::string& name);
    
    void clear();
    void stopAll();
    void cleanup();

    void playPreview(const std::string& fullPath);
    void stopPreview();

    uint64_t getCurrentMemory() const { return currentTotalMemory; }
    uint64_t getMaxMemory() const { return MAX_WAV_MEMORY; }

    // --- ヘルパー: 文字列からのID生成（一貫性維持用） ---
    inline uint32_t getHash(const std::string& name) const {
        return std::hash<std::string>{}(name);
    }

private:
    SoundManager() : currentPreviewChunk(nullptr), currentTotalMemory(0) {} 
    ~SoundManager() { cleanup(); }
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    struct BoxEntry {
        std::string pckPath;
        uint32_t offset;
        uint32_t size;
    };

    // --- 最適化: キーを std::string から uint32_t (ハッシュID) に変更 ---
    // これにより PlayableNote のコピーから std::string が消え、演奏中の検索が高速化されます
    std::unordered_map<uint32_t, Mix_Chunk*> sounds;
    std::unordered_map<uint32_t, int> activeChannels;
    
    // ロード時にファイル名で検索する必要があるため、ここは string を維持
    std::unordered_map<std::string, BoxEntry> boxIndex;

    Mix_Chunk* currentPreviewChunk = nullptr;

    uint64_t currentTotalMemory = 0;
    const uint64_t MAX_WAV_MEMORY = 512 * 1024 * 1024; 
};

#endif