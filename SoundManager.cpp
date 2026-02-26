#include "SoundManager.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>

/**
 * SDL_mixerの初期化とチャンネルの割り当て
 */
void SoundManager::init() {
    sounds.reserve(4000);
    SDL_SetHint("SDL_AUDIO_RESAMPLING_MODE", "linear");

    // バッファサイズを 1024 に設定
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        std::cerr << "Mix_OpenAudio Error: " << Mix_GetError() << std::endl;
        // フォールバック
        Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 1024);
    }

    Mix_AllocateChannels(256);
    std::cout << "SoundManager Initialized." << std::endl;
}

void SoundManager::preloadBoxIndex(const std::string& rootPath, const std::string& bmsonName) {
    boxIndex.clear();
    int partIdx = 1;
    while (true) {
        std::string suffix = (partIdx == 1) ? "" : std::to_string(partIdx);
        std::string pckPath = rootPath + (rootPath.empty() || rootPath.back() == '/' ? "" : "/") + bmsonName + suffix + ".boxwav";
        
        std::ifstream ifs(pckPath, std::ios::binary);
        if (!ifs) {
            break;
        }

        uint32_t count, d1, d2;
        if (!ifs.read((char*)&count, 4)) break;
        ifs.read((char*)&d1, 4);
        ifs.read((char*)&d2, 4);

        for (uint32_t i = 0; i < count; ++i) {
            char nameBuf[32];
            uint32_t fSize;
            if (!ifs.read(nameBuf, 32)) break;
            if (!ifs.read((char*)&fSize, 4)) break;

            std::string fileName(nameBuf, strnlen(nameBuf, 32));
            boxIndex[fileName] = { pckPath, (uint32_t)ifs.tellg(), fSize };
            ifs.seekg(fSize, std::ios::cur);
        }
        partIdx++;
        if (partIdx > 128) break;
    }
}

void SoundManager::loadSingleSound(const std::string& filename, const std::string& rootPath, const std::string& bmsonName) {
    uint32_t id = getHash(filename);
    if (sounds.find(id) != sounds.end()) return;

    if (boxIndex.count(filename)) {
        auto& entry = boxIndex[filename];
        // 1. ファイルを直接RWopsとして開く (メモリにコピーしない)
        // boxwavを共有するため、ここではLoadWAV_RWに所有権を渡さず(0)、手動で管理する
        SDL_RWops* rw = SDL_RWFromFile(entry.pckPath.c_str(), "rb");
        if (rw) {
            SDL_RWseek(rw, entry.offset, RW_SEEK_SET);
            // Mix_LoadWAV_RW(rw, 0) で rw を閉じずに利用
            // 注意: SDL_mixerのバージョンにより、特定範囲のみを読み込ませるには
            // 本来はカスタムRWopsが必要ですが、最小変更として一旦rwを渡します
            Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 0); 
            if (chunk) {
                sounds[id] = chunk;
                currentTotalMemory += entry.size;
            } else {
                std::cerr << "Memory Error or Invalid WAV: " << filename << " (" << Mix_GetError() << ")" << std::endl;
            }
            SDL_RWclose(rw); // 使い終わったら即座に閉じる
            return; 
        }
    }

    // 外部ファイル読み込み
    std::string path = rootPath + (rootPath.empty() || rootPath.back() == '/' ? "" : "/") + filename;
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return;

    uint64_t fileSize = SDL_RWsize(rw);
    if (currentTotalMemory + fileSize > MAX_WAV_MEMORY) {
        SDL_RWclose(rw);
        return;
    }

    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1); // ここは1で所有権を渡して自動クローズ
    if (chunk) {
        sounds[id] = chunk;
        currentTotalMemory += fileSize;
    }
}

void SoundManager::loadSoundsInBulk(const std::vector<std::string>& filenames, 
                                    const std::string& rootPath, 
                                    const std::string& bmsonName,
                                    std::function<void(int, const std::string&)> onProgress) {
    
    std::unordered_map<std::string, std::vector<std::string>> groupPerBox;
    std::vector<std::string> externalFiles;

    for (const auto& name : filenames) {
        if (sounds.find(getHash(name)) != sounds.end()) continue;
        if (boxIndex.count(name)) {
            groupPerBox[boxIndex[name].pckPath].push_back(name);
        } else {
            externalFiles.push_back(name);
        }
    }

    int processedCount = 0;

    for (auto& [pckPath, list] : groupPerBox) {
        // boxwavを一度だけ開く
        SDL_RWops* rwBox = SDL_RWFromFile(pckPath.c_str(), "rb");
        if (!rwBox) continue;

        std::sort(list.begin(), list.end(), [&](const std::string& a, const std::string& b) {
            return boxIndex[a].offset < boxIndex[b].offset;
        });

        for (const auto& name : list) {
            auto& entry = boxIndex[name];
            if (currentTotalMemory + entry.size > MAX_WAV_MEMORY) {
                processedCount++;
                continue;
            }

            // シークして直接展開
            SDL_RWseek(rwBox, entry.offset, RW_SEEK_SET);
            Mix_Chunk* chunk = Mix_LoadWAV_RW(rwBox, 0); // 0 = rwBoxを閉じない
            
            if (chunk) {
                sounds[getHash(name)] = chunk;
                currentTotalMemory += entry.size;
            }

            processedCount++;
            if (onProgress) onProgress(processedCount, name);
        }
        SDL_RWclose(rwBox); // 全ファイル読み終わったら閉じる
    }

    for (const auto& name : externalFiles) {
        loadSingleSound(name, rootPath, bmsonName);
        processedCount++;
        if (onProgress) onProgress(processedCount, name);
    }
}

// --- play 関数の重複を削除 ---
// 以前の void SoundManager::play(int channel_id) { ... } は完全に消去してください。

void SoundManager::play(int soundId) { 
    uint32_t id = static_cast<uint32_t>(soundId);
    if (sounds.count(id) && sounds[id] != nullptr) {
        Mix_Chunk* targetChunk = sounds[id];
        int newChannel = Mix_PlayChannel(-1, targetChunk, 0);

        if (newChannel == -1) {
            static int nextVictim = 0;
            newChannel = nextVictim;
            Mix_HaltChannel(newChannel);
            Mix_PlayChannel(newChannel, targetChunk, 0);
            nextVictim = (nextVictim + 1) % 256; 
        }

        if (newChannel != -1) {
            Mix_Volume(newChannel, 96);
            activeChannels[id] = newChannel;
        }
    }
}

void SoundManager::playByName(const std::string& name) {
    play(static_cast<int>(getHash(name)));
}

void SoundManager::playPreview(const std::string& fullPath) {
    static std::string lastPath = "";
    if (lastPath == fullPath && currentPreviewChunk != nullptr) return;
    stopPreview();

    SDL_RWops* rw = SDL_RWFromFile(fullPath.c_str(), "rb");
    if (!rw) return;

    Mix_Chunk* previewChunk = Mix_LoadWAV_RW(rw, 1);
    if (previewChunk) {
        Mix_PlayChannel(255, previewChunk, -1);
        Mix_Volume(255, 80); 
        currentPreviewChunk = previewChunk;
        lastPath = fullPath;
    }
}

void SoundManager::stopPreview() {
    Mix_HaltChannel(255);
    if (currentPreviewChunk) {
        Mix_FreeChunk(currentPreviewChunk);
        currentPreviewChunk = nullptr;
    }
}

void SoundManager::stopAll() {
    Mix_HaltChannel(-1);
    activeChannels.clear();
    stopPreview();
}

void SoundManager::clear() {
    stopAll();
    for (auto& pair : sounds) {
        if (pair.second) Mix_FreeChunk(pair.second);
    }
    // ★エラー修正：キーの型を uint32_t に合わせる
    std::unordered_map<uint32_t, Mix_Chunk*>().swap(sounds);
    std::unordered_map<uint32_t, int>().swap(activeChannels);
    
    boxIndex.clear();
    currentTotalMemory = 0;
}

void SoundManager::cleanup() {
    clear();
    Mix_CloseAudio(); // アプリ終了時のみ閉じる
}