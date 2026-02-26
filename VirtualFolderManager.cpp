#include "VirtualFolderManager.hpp"
#include "ScoreManager.hpp"
#include "Config.hpp" // 追加
#include <map>
#include <algorithm>
#include <cctype>

std::vector<SongGroup> VirtualFolderManager::createCustomFolders(const std::vector<SongEntry>& songCache) {
    std::vector<SongGroup> customFolders;

    // レベル別フォルダを取得 (既存ロジック継承 + 表示フラグ判定)
    if (Config::SHOW_LEVEL_FOLDER) {
        auto levelFolders = createLevelFolders(songCache);
        customFolders.insert(customFolders.end(), levelFolders.begin(), levelFolders.end());
    }

    // クリアランプ別フォルダを取得 (既存ロジック継承 + 表示フラグ判定)
    if (Config::SHOW_LAMP_FOLDER) {
        auto lampFolders = createLampFolders(songCache);
        customFolders.insert(customFolders.end(), lampFolders.begin(), lampFolders.end());
    }

    // 【追加】DJランク別フォルダを取得 (既存ロジック継承 + 表示フラグ判定)
    if (Config::SHOW_RANK_FOLDER) {
        auto rankFolders = createRankFolders(songCache);
        customFolders.insert(customFolders.end(), rankFolders.begin(), rankFolders.end());
    }

    // 難易度種別フォルダを取得 (新規追加 + 表示フラグ判定)
    if (Config::SHOW_CHART_TYPE_FOLDER) {
        auto chartFolders = createChartTypeFolders(songCache);
        customFolders.insert(customFolders.end(), chartFolders.begin(), chartFolders.end());
    }

    // ノーツ数範囲フォルダを取得 (新規追加 + 表示フラグ判定)
    if (Config::SHOW_NOTES_RANGE_FOLDER) {
        auto notesFolder = createNotesRangeFolder(songCache);
        if (!notesFolder.songIndices.empty()) {
            customFolders.push_back(notesFolder);
        }
    }

    // 先頭文字フォルダを取得 (既存ロジック継承 + 表示フラグ判定)
    if (Config::SHOW_ALPHA_FOLDER) {
        auto alphaFolders = createAlphaFolders(songCache);
        customFolders.insert(customFolders.end(), alphaFolders.begin(), alphaFolders.end());
    }
    
    return customFolders;
}

std::vector<SongGroup> VirtualFolderManager::createLevelFolders(const std::vector<SongEntry>& songCache) {
    // key: レベル数値, value: そのレベルに属する曲の songCache 内のインデックス
    std::map<int, std::vector<int>> levelMap;

    for (int i = 0; i < (int)songCache.size(); ++i) {
        int lv = songCache[i].level;
        levelMap[lv].push_back(i);
    }

    std::vector<SongGroup> groups;
    for (auto const& [lv, indices] : levelMap) {
        SongGroup g;
        g.title = "Level " + std::to_string(lv);
        g.sortName = std::string("!LV_") + (lv < 10 ? "0" : "") + std::to_string(lv); // "!01", "!12" のようにして先頭に並べる
        g.songIndices = indices;
        g.isFolder = true;
        g.folderPath = "VIRTUAL:LEVEL_" + std::to_string(lv); // 仮想パス
        
        groups.push_back(g);
    }

    return groups;
}

// クリアランプ別フォルダ生成 (既存ロジック継承)
std::vector<SongGroup> VirtualFolderManager::createLampFolders(const std::vector<SongEntry>& songCache) {
    std::map<ClearType, std::vector<int>> lampMap;

    for (int i = 0; i < (int)songCache.size(); ++i) {
        const auto& entry = songCache[i];
        // 既存の ScoreManager を利用して最新のクリアランプを取得
        BestScore score = ScoreManager::loadScore(entry.title, entry.chartName, (int)entry.total);
        lampMap[score.clearType].push_back(i);
    }

    // 表示順と名称の定義
    struct LampDef {
        ClearType type;
        std::string name;
        std::string sortSuffix;
    };

    std::vector<LampDef> defs = {
        { ClearType::NO_PLAY,        "NO PLAY",       "0" },
        { ClearType::FAILED,         "FAILED",        "1" },
        { ClearType::DAN_CLEAR,      "DAN CLEAR",     "2" },
        { ClearType::ASSIST_CLEAR,   "ASSIST CLEAR",  "3" },
        { ClearType::EASY_CLEAR,     "EASY CLEAR",    "4" },
        { ClearType::NORMAL_CLEAR,   "CLEAR",         "5" },
        { ClearType::HARD_CLEAR,     "HARD CLEAR",    "6" },
        { ClearType::EX_HARD_CLEAR,  "EX HARD CLEAR", "7" },
        { ClearType::FULL_COMBO,     "FULL COMBO",    "8" }
    };

    std::vector<SongGroup> groups;
    for (const auto& def : defs) {
        if (lampMap[def.type].empty()) continue; 

        SongGroup g;
        g.title = def.name;
        g.sortName = "!Z_LAMP_" + def.sortSuffix;
        g.songIndices = lampMap[def.type];
        g.isFolder = true;
        g.folderPath = "VIRTUAL:LAMP_" + std::to_string((int)def.type);
        
        groups.push_back(g);
    }

    return groups;
}

// 【追加】DJランク（スコア）別フォルダ生成
std::vector<SongGroup> VirtualFolderManager::createRankFolders(const std::vector<SongEntry>& songCache) {
    std::map<std::string, std::vector<int>> rankMap;

    for (int i = 0; i < (int)songCache.size(); ++i) {
        const auto& entry = songCache[i];
        BestScore score = ScoreManager::loadScore(entry.title, entry.chartName, (int)entry.total);
        std::string r = score.rank;
        if (r.empty()) r = "NO PLAY";
        rankMap[r].push_back(i);
    }

    // ランクの表示順序
    std::vector<std::string> rankOrder = {"AAA", "AA", "A", "B", "C", "D", "E", "F", "NO PLAY"};
    std::vector<SongGroup> groups;

    for (int i = 0; i < (int)rankOrder.size(); ++i) {
        const std::string& rName = rankOrder[i];
        if (rankMap[rName].empty()) continue;

        SongGroup g;
        g.title = "DJ RANK " + rName;
        g.sortName = "!Z_RANK_" + std::to_string(i); // ランプフォルダの付近に並べる
        g.songIndices = rankMap[rName];
        g.isFolder = true;
        g.folderPath = "VIRTUAL:RANK_" + rName;
        groups.push_back(g);
    }

    return groups;
}

// 難易度種別フォルダ生成 (新規追加：bmsonloader等の判定を基準に分類)
std::vector<SongGroup> VirtualFolderManager::createChartTypeFolders(const std::vector<SongEntry>& songCache) {
    std::map<std::string, std::vector<int>> chartMap;
    
    for (int i = 0; i < (int)songCache.size(); ++i) {
        std::string name = songCache[i].chartName;
        std::transform(name.begin(), name.end(), name.begin(), ::toupper);

        // 判定ロジック：特定のキーワードが含まれているかで分類
        if (name.find("LEGGENDARIA") != std::string::npos || name.find("†") != std::string::npos)
            chartMap["LEGGENDARIA"].push_back(i);
        else if (name.find("ANOTHER") != std::string::npos)
            chartMap["ANOTHER"].push_back(i);
        else if (name.find("HYPER") != std::string::npos)
            chartMap["HYPER"].push_back(i);
        else if (name.find("NORMAL") != std::string::npos)
            chartMap["NORMAL"].push_back(i);
        else if (name.find("BEGINNER") != std::string::npos)
            chartMap["BEGINNER"].push_back(i);
        else
            chartMap["OTHERS"].push_back(i);
    }

    // 表示順の定義
    std::vector<std::string> order = {"BEGINNER", "NORMAL", "HYPER", "ANOTHER", "LEGGENDARIA", "OTHERS"};
    std::vector<SongGroup> groups;
    
    for (int i = 0; i < (int)order.size(); ++i) {
        if (chartMap[order[i]].empty()) continue;

        SongGroup g;
        g.title = order[i];
        g.sortName = "!Z_CHART_" + std::to_string(i); // ランプフォルダの後に並ぶよう調整
        g.songIndices = chartMap[order[i]];
        g.isFolder = true;
        g.folderPath = "VIRTUAL:CHART_" + order[i];
        groups.push_back(g);
    }
    return groups;
}

// ノーツ数範囲フォルダ生成 (新規追加：ユーザー指定の単一フォルダ)
SongGroup VirtualFolderManager::createNotesRangeFolder(const std::vector<SongEntry>& songCache) {
    SongGroup g;
    int minN = Config::FOLDER_NOTES_MIN; // Config.hppに定義が必要
    int maxN = Config::FOLDER_NOTES_MAX; // Config.hppに定義が必要

    for (int i = 0; i < (int)songCache.size(); ++i) {
        if (songCache[i].total >= (uint32_t)minN && songCache[i].total <= (uint32_t)maxN) {
            g.songIndices.push_back(i);
        }
    }

    if (!g.songIndices.empty()) {
        g.title = "NOTES: " + std::to_string(minN) + " - " + std::to_string(maxN);
        g.sortName = "!Z_NOTES_SINGLE"; // 難易度フォルダの後に配置
        g.isFolder = true;
        g.folderPath = "VIRTUAL:NOTES_FILTER";
    }
    return g;
}

// 先頭文字フォルダ生成 (既存ロジック継承)
std::vector<SongGroup> VirtualFolderManager::createAlphaFolders(const std::vector<SongEntry>& songCache) {
    struct AlphaDef {
        std::string name;
        std::string targetChars;
        std::vector<int> songIndices;
    };

    std::vector<AlphaDef> defs = {
        {"ABCD", "ABCD", {}},
        {"EFGH", "EFGH", {}},
        {"IJKL", "IJKL", {}},
        {"MNOP", "MNOP", {}},
        {"QRST", "QRST", {}},
        {"UVWX", "UVWX", {}},
        {"YZ",   "YZ",   {}},
        {"OTHER", "",    {}} // それ以外
    };

    for (int i = 0; i < (int)songCache.size(); ++i) {
        if (songCache[i].title.empty()) {
            defs.back().songIndices.push_back(i);
            continue;
        }

        char first = (char)std::toupper((unsigned char)songCache[i].title[0]);
        bool matched = false;

        for (int d = 0; d < (int)defs.size() - 1; ++d) {
            if (defs[d].targetChars.find(first) != std::string::npos) {
                defs[d].songIndices.push_back(i);
                matched = true;
                break;
            }
        }

        if (!matched) {
            defs.back().songIndices.push_back(i);
        }
    }

    std::vector<SongGroup> groups;
    for (int i = 0; i < (int)defs.size(); ++i) {
        if (defs[i].songIndices.empty()) continue;

        SongGroup g;
        g.title = defs[i].name;
        // 最も後ろに並ぶように "!Z_TITLE_" を使用
        g.sortName = "!Z_TITLE_" + std::to_string(i);
        g.songIndices = defs[i].songIndices;
        g.isFolder = true;
        g.folderPath = "VIRTUAL:ALPHA_" + defs[i].name;
        
        groups.push_back(g);
    }

    return groups;
}