#include "SoundManager.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <cstring>

/**
 * SDL_mixerの初期化とチャンネルの割り当て (100%継承)
 */
void SoundManager::init() {
    sounds.reserve(4000);
    SDL_SetHint("SDL_AUDIO_RESAMPLING_MODE", "linear");

    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 512) < 0) {
        std::cerr << "Mix_OpenAudio Error: " << Mix_GetError() << std::endl;
    }

    Mix_AllocateChannels(256);
    std::cout << "SoundManager Initialized: 22050Hz (Mono), Buffer 512, Channels 256, Reserve 4000." << std::endl;
}

/**
 * 楽曲ロード開始時にboxwav内の全ファイルの位置を記録する
 * 各パートのロード間に待機を入れ、SwitchのI/O負荷を軽減します。
 */
void SoundManager::preloadBoxIndex(const std::string& rootPath, const std::string& bmsonName) {
    boxIndex.clear();
    int partIdx = 1;
    while (true) {
        std::string suffix = (partIdx == 1) ? "" : std::to_string(partIdx);
        std::string pckPath = rootPath + (rootPath.empty() || rootPath.back() == '/' ? "" : "/") + bmsonName + suffix + ".boxwav";
        
        // --- 【既存ロジック維持】パート2以降の待機 ---
        if (partIdx > 1) {
            std::cout << "Part " << partIdx << " detected. Waiting for I/O settle..." << std::endl;
            SDL_Delay(2500); 
        }

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

            boxIndex[std::string(nameBuf)] = { pckPath, (uint32_t)ifs.tellg(), fSize };
            ifs.seekg(fSize, std::ios::cur);
        }

        std::cout << "BoxWav Part " << partIdx << " Indexed: " << pckPath << std::endl;

        partIdx++;
        if (partIdx > 128) break;
    }
    std::cout << "BoxWav Indexing complete: " << boxIndex.size() << " files indexed." << std::endl;
}

/**
 * 単一の音源ファイルをロード
 * 一時バッファ(vector)を限定スコープに閉じ込め、メモリピークを抑制
 */
void SoundManager::loadSingleSound(const std::string& filename, const std::string& rootPath, const std::string& bmsonName) {
    if (sounds.find(filename) != sounds.end()) return;

    // --- 1. インデックスからバイナリ検索 ---
    if (boxIndex.count(filename)) {
        auto& entry = boxIndex[filename];
        
        if (currentTotalMemory + entry.size > MAX_WAV_MEMORY) {
            return;
        }

        std::ifstream ifs(entry.pckPath, std::ios::binary);
        if (ifs) {
            ifs.seekg(entry.offset, std::ios::beg);
            
            {
                std::vector<uint8_t> buffer(entry.size);
                ifs.read((char*)buffer.data(), entry.size);
                ifs.close(); 

                SDL_RWops* rw = SDL_RWFromMem(buffer.data(), entry.size);
                Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1); 
                if (chunk) {
                    sounds[filename] = chunk;
                    currentTotalMemory += entry.size;
                }
            } 
            return; 
        }
    }

    // --- 2. 【通常ロード】インデックスになかった場合 ---
    std::string path = rootPath + (rootPath.empty() || rootPath.back() == '/' ? "" : "/") + filename;
    SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
    if (!rw) return;

    uint64_t fileSize = SDL_RWsize(rw);
    if (currentTotalMemory + fileSize > MAX_WAV_MEMORY) {
        SDL_RWclose(rw);
        return;
    }

    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1);
    if (chunk) {
        sounds[filename] = chunk;
        currentTotalMemory += fileSize;
    }
}

/**
 * 指定したチャンネルIDで再生 (インターフェース維持)
 */
void SoundManager::play(int channel_id) { 
    /* 100%継承 */ 
}

/**
 * ファイル名をキーにして音を再生 (既存ロジック100%継承)
 */
void SoundManager::playByName(const std::string& name) {
    if (sounds.count(name) && sounds[name] != nullptr) {
        Mix_Chunk* targetChunk = sounds[name];
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
            activeChannels[name] = newChannel;
        }
    }
}

/**
 * 【追加】選曲画面用：プレビュー音楽の再生 (安全強化版)
 */
void SoundManager::playPreview(const std::string& fullPath) {
    // 1. 同一パスなら何もしない (無駄なロードを回避)
    static std::string lastPath = "";
    if (lastPath == fullPath && currentPreviewChunk != nullptr) return;

    // 2. 既存のプレビューがあれば確実に停止・解放
    stopPreview();

    // 3. ファイルが存在するかチェックしてからロード
    SDL_RWops* rw = SDL_RWFromFile(fullPath.c_str(), "rb");
    if (!rw) return;

    Mix_Chunk* previewChunk = Mix_LoadWAV_RW(rw, 1); // 1でRWopsを自動解放
    if (previewChunk) {
        // チャンネル255番をプレビュー専用として使用
        int channel = 255;
        Mix_PlayChannel(channel, previewChunk, -1); // ループ再生
        Mix_Volume(channel, 80); 
        
        currentPreviewChunk = previewChunk;
        lastPath = fullPath;
    }
}

/**
 * 【追加】選曲画面用：プレビュー音楽の停止
 */
void SoundManager::stopPreview() {
    Mix_HaltChannel(255);
    if (currentPreviewChunk) {
        Mix_FreeChunk(currentPreviewChunk);
        currentPreviewChunk = nullptr;
    }
}

/**
 * 再生中のすべての音を即座に停止
 */
void SoundManager::stopAll() {
    Mix_HaltChannel(-1);
    activeChannels.clear();
    stopPreview(); // プレビューも止める
}

/**
 * 再生を停止し、全メモリを解放 (100%継承)
 */
void SoundManager::clear() {
    stopAll();
    for (auto& pair : sounds) {
        if (pair.second) {
            Mix_FreeChunk(pair.second);
            pair.second = nullptr;
        }
    }
    
    std::unordered_map<std::string, Mix_Chunk*>().swap(sounds);
    std::unordered_map<std::string, int>().swap(activeChannels);
    boxIndex.clear();
    currentTotalMemory = 0;
    sounds.reserve(4000);

    Mix_CloseAudio();
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 512) < 0) {
        std::cerr << "Mix_OpenAudio Error during clear: " << Mix_GetError() << std::endl;
    }
    Mix_AllocateChannels(256);
}

/**
 * SDL_mixerを終了
 */
void SoundManager::cleanup() {
    clear();
    Mix_CloseAudio();
}