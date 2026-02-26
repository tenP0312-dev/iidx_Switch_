#ifndef SCOREMANAGER_HPP
#define SCOREMANAGER_HPP

#include <string>
#include <map> // 追加: キャッシュ管理用
#include "CommonTypes.hpp"

class ScoreManager {
public:
    /**
     * @brief 楽曲情報に基づいてベストスコアを保存します。
     * @param title 楽曲タイトル
     * @param chartName 難易度名 (NORMAL, ANOTHER等)
     * @param totalNotes 総ノーツ数 (差分判別用)
     * @param status 今回のプレイ結果
     */
    static void saveIfBest(const std::string& title, const std::string& chartName, int totalNotes, const PlayStatus& status);

    /**
     * @brief 楽曲情報に基づいてベストスコアを読み込みます。
     * @param title 楽曲タイトル
     * @param chartName 難易度名
     * @param totalNotes 総ノーツ数
     * @return 読み込んだベストスコア（ファイルがない場合は初期値を返す）
     */
    static BestScore loadScore(const std::string& title, const std::string& chartName, int totalNotes);

    /**
     * @brief 【追加】キャッシュをクリアします（リスキャン時用）
     */
    static void clearCache() { scoreCache.clear(); }

private:
    /**
     * @brief EXスコアを計算します (P-GREAT*2 + GREAT*1)
     */
    static int calculateExScore(int pg, int gr);

    /**
     * @brief ランクを計算します (スコア率に基づく)
     */
    static std::string calculateRank(int exScore, int totalNotes);

    /**
     * @brief 【追加】文字列を安全なハッシュ値（英数字）に変換します
     */
    static std::string convertToHash(const std::string& input);

    /**
     * @brief 与えられた情報からユニークなID（ファイル名用）を生成します
     */
    static std::string generateUniqueId(const std::string& title, const std::string& chartName, int totalNotes);

    /**
     * @brief 保存先のファイルパスを取得します
     */
    static std::string getSavePath(const std::string& uniqueId);

    /**
     * @brief 【追加】スコアをメモリに保持するキャッシュ
     */
    static std::map<std::string, BestScore> scoreCache;
};

#endif