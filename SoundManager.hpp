#ifndef SOUNDMANAGER_HPP
#define SOUNDMANAGER_HPP

#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <cstdint> 
#include <vector>
#include <functional> // 追加: std::function用

class SoundManager {
public:
    static SoundManager& getInstance() {
        static SoundManager instance;
        return instance;
    }

    void init();
    
    void loadSingleSound(const std::string& filename, const std::string& rootPath, const std::string& bmsonName = "sounds");
    
    // --- 【追加】一括ロード機能 (進捗コールバック付) ---
    void loadSoundsInBulk(const std::vector<std::string>& filenames, 
                          const std::string& rootPath, 
                          const std::string& bmsonName,
                          std::function<void(int, const std::string&)> onProgress = nullptr);

    void preloadBoxIndex(const std::string& rootPath, const std::string& bmsonName);

    void play(int channel_id);
    void playByName(const std::string& name);
    void clear();
    void stopAll();
    void cleanup();

    void playPreview(const std::string& fullPath);
    void stopPreview();

    uint64_t getCurrentMemory() const { return currentTotalMemory; }
    uint64_t getMaxMemory() const { return MAX_WAV_MEMORY; }

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

    std::unordered_map<std::string, Mix_Chunk*> sounds;
    std::unordered_map<std::string, int> activeChannels;
    std::unordered_map<std::string, BoxEntry> boxIndex;

    Mix_Chunk* currentPreviewChunk = nullptr;

    uint64_t currentTotalMemory = 0;
    const uint64_t MAX_WAV_MEMORY = 512 * 1024 * 1024; 
};

#endif