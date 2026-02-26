#ifndef SONG_MANAGER_HPP
#define SONG_MANAGER_HPP

#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <SDL2/SDL.h> // 追加: SDL_Renderer用
#include "CommonTypes.hpp"
#include "BMSData.hpp"
#include "NoteRenderer.hpp" // 追加: NoteRenderer用
// 【修正】SongEntry と SongGroup の実体定義を読み込むために追加
#include "SceneSelect.hpp" 

namespace fs = std::filesystem;

class SongManager {
public:
    /**
     * @brief 楽曲リストの読み込み（キャッシュ利用または再スキャン）
     * 既存ロジック100%継承 + フォルダ階層フィルタリング対応
     * 修正：描画用に SDL_Renderer* と NoteRenderer& を追加
     */
    static void loadSongList(std::vector<SongEntry>& songCache, std::vector<SongGroup>& songGroups, bool forceScan, const std::string& currentPath, SDL_Renderer* ren, NoteRenderer& renderer);

    /**
     * @brief 難易度スロット(0-4)を取得する
     */
    static int getDifficultyOrder(const std::string& chartName);

    /**
     * @brief 各グループの難易度をユーザーの好みに同期する
     */
    static void syncDifficulty(std::vector<SongGroup>& songGroups, const std::vector<SongEntry>& songCache, int preferredSlot);

    /**
     * @brief 【追加】キャッシュをクリアします（強制リスキャン時用）
     */
    static void clearCache() { 
        folderCustomCache.clear(); 
    }

private:
    /**
     * @brief 再帰的にbmsonを探す
     * 修正：描画用に SDL_Renderer* と NoteRenderer& を追加
     */
    static void scanBmsonRecursive(const std::string& path, std::vector<SongEntry>& songCache, SDL_Renderer* ren, NoteRenderer& renderer);

    /**
     * @brief 【追加】フォルダカスタム設定ファイルをパースする
     * メンバ関数にすることで private な folderCustomCache にアクセス可能にする
     */
    static void parseFolderCustom(const fs::path& folderPath, const std::string& folderName, SongGroup& group);

    /**
     * @brief 【追加】フォルダごとのカスタム設定（画像・音）を保持するキャッシュ
     */
    static std::map<std::string, SongGroup> folderCustomCache;
};

#endif