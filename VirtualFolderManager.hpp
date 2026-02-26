#ifndef VIRTUALFOLDERMANAGER_HPP
#define VIRTUALFOLDERMANAGER_HPP

#include <vector>
#include <string>
#include "SceneSelect.hpp" // SongEntry, SongGroup の定義を使用

class VirtualFolderManager {
public:
    /**
     * @brief 全楽曲キャッシュから仮想フォルダ（レベル別など）を生成します
     * @param songCache 全楽曲のリスト
     * @return 生成された仮想フォルダのリスト
     */
    static std::vector<SongGroup> createCustomFolders(const std::vector<SongEntry>& songCache);

private:
    /**
     * @brief レベル別に曲を振り分けたフォルダを作成します
     */
    static std::vector<SongGroup> createLevelFolders(const std::vector<SongEntry>& songCache);

    /**
     * @brief クリアランプ別に曲を振り分けたフォルダを作成します
     */
    static std::vector<SongGroup> createLampFolders(const std::vector<SongEntry>& songCache);

    /**
     * @brief DJランク別に曲を振り分けたフォルダを作成します (エラー解消のため追加)
     */
    static std::vector<SongGroup> createRankFolders(const std::vector<SongEntry>& songCache);

    /**
     * @brief 先頭文字別に曲を振り分けたフォルダを作成します
     */
    static std::vector<SongGroup> createAlphaFolders(const std::vector<SongEntry>& songCache);

    // --- エラー解消のために追加 (Minimal Invasive Change) ---
    /**
     * @brief 譜面タイプ別に曲を振り分けたフォルダを作成します
     */
    static std::vector<SongGroup> createChartTypeFolders(const std::vector<SongEntry>& songCache);

    /**
     * @brief ノーツ数範囲別に曲を振り分けたフォルダを作成します
     */
    static SongGroup createNotesRangeFolder(const std::vector<SongEntry>& songCache);
    // -----------------------------------------------------

};

#endif