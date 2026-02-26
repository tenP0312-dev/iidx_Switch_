#include "BmsonLoader.hpp"
#include <fstream>
#include <algorithm>
#include "SoundManager.hpp" // SoundManagerへのアクセス用

// 文字列を安全に取得する補助関数（null対策）
std::string get_string_safe(const nlohmann::json& j, const std::string& key, const std::string& def) {
    if (!j.contains(key) || j[key].is_null() || !j[key].is_string()) return def;
    return j[key].get<std::string>();
}

// 数値を安全に取得する補助関数（null・型違い対策）
double get_double_safe(const nlohmann::json& j, const std::string& key, double default_val) {
    if (!j.contains(key) || j[key].is_null()) return default_val;
    auto& v = j[key];
    if (v.is_number()) return v.get<double>();
    if (v.is_string()) { try { return std::stod(v.get<std::string>()); } catch (...) { return default_val; } }
    return default_val;
}

// 画像仕様に基づき、プレイヤー1の範囲(1-8)を判定
bool isPlayableLaneSP(int64_t x) {
    return (x >= 1 && x <= 8);
}

BMSData BmsonLoader::load(const std::string& path, std::function<void(float)> onProgress) {
    BMSData data;
    std::string rootDir = path.substr(0, path.find_last_of("/\\") + 1);

    // 1. JSONパース用のスコープを限定する
    {
        std::ifstream f(path);
        if (!f.is_open()) return data;
        
        nlohmann::json j;
        try {
            f >> j;
            // メモリ節約のため、ファイルを即座に閉じる
            f.close();

            const nlohmann::json& info = (j.contains("info") && !j["info"].is_null()) ? j["info"] : j;
            
            // --- 基本ヘッダ情報の抽出 ---
            data.header.title = get_string_safe(info, "title", "Unknown");
            data.header.artist = get_string_safe(info, "artist", "Unknown");
            data.header.genre = get_string_safe(info, "genre", "Unknown");
            data.header.modeHint = get_string_safe(info, "mode_hint", "");
            data.header.subtitle = get_string_safe(info, "subtitle", "");
            
            // ... (中略: 難易度判定ロジックは既存のものをそのまま維持) ...
            // [難易度・BPM計算ロジックは既存コードを100%継承]
            
            double bpm = get_double_safe(info, "bpm", -1.0);
            if (bpm <= 0) bpm = get_double_safe(info, "init_bpm", -1.0);
            if (bpm <= 0) bpm = get_double_safe(j, "bpm", -1.0);
            if (bpm <= 0) bpm = get_double_safe(j, "init_bpm", 120.0);
            data.header.bpm = bpm;
            data.header.resolution = (int)get_double_safe(info, "resolution", 480.0);
            data.header.min_bpm = bpm;
            data.header.max_bpm = bpm;
            data.bpm_events.push_back({0, bpm});

            const nlohmann::json& bpm_src = j.contains("bpm_events") ? j["bpm_events"] : (info.contains("bpm_events") ? info["bpm_events"] : nlohmann::json());
            if (bpm_src.is_array()) {
                for (auto& e : bpm_src) {
                    int64_t y = e.value("y", (int64_t)0);
                    double b = get_double_safe(e, "bpm", data.header.bpm);
                    if (y == 0) data.bpm_events[0].bpm = b;
                    else data.bpm_events.push_back({y, b});
                    if (b < data.header.min_bpm) data.header.min_bpm = b;
                    if (b > data.header.max_bpm) data.header.max_bpm = b;
                }
            }
            std::sort(data.bpm_events.begin(), data.bpm_events.end(), [](auto& a, auto& b){ return a.y < b.y; });

            // --- 音源・ノーツ情報の抽出 ---
            int totalNotesCount = 0;
            bool hasP1_6or7 = false;
            bool hasP2Side = false;

            const nlohmann::json& sc_src = j.contains("sound_channels") ? j["sound_channels"] : (info.contains("sound_channels") ? info["sound_channels"] : nlohmann::json());
            if (sc_src.is_array()) {
                size_t totalCh = sc_src.size();
                data.sound_channels.reserve(totalCh); // 事前に予約して再確保を減らす
                for (size_t i = 0; i < totalCh; ++i) {
                    const auto& ch = sc_src[i];
                    BMSSoundChannel channel;
                    channel.name = get_string_safe(ch, "name", "");
                    
                    if (ch.contains("notes") && ch["notes"].is_array()) {
                        const auto& notes_arr = ch["notes"];
                        channel.notes.reserve(notes_arr.size());
                        for (auto& n : notes_arr) {
                            int64_t x = n.value("x", (int64_t)0);
                            int64_t y = n.value("y", (int64_t)0);
                            int64_t l = n.value("l", (int64_t)0);
                            channel.notes.push_back({x, y, l});

                            if (x == 6 || x == 7) hasP1_6or7 = true;
                            if (x >= 9 && x <= 16) hasP2Side = true;
                            if (isPlayableLaneSP(x)) totalNotesCount++;
                        }
                    }
                    data.sound_channels.push_back(std::move(channel)); // moveでコピーを避ける

                    if (onProgress && i % 10 == 0) onProgress((float)(i + 1) / (float)totalCh);
                }
            }
            data.header.totalNotes = totalNotesCount;
            data.header.is7Key = (hasP1_6or7 && !hasP2Side);

            // --- BGAデータの抽出 ---
            // [既存のBGAパースロジックをそのままここに配置]
            // ... (既存の bga_node 処理) ...

        } catch (...) {
            // エラー時も例外を投げず、それまでのデータを返す
        }
        
        // ここで j のスコープが終了し、巨大な JSON メモリが完全に解放される
    } 

    // 2. メモリの「断片化」対策：不要な予約領域をOSに返却する
    data.sound_channels.shrink_to_fit();
    for(auto& ch : data.sound_channels) {
        ch.notes.shrink_to_fit();
    }
    data.bpm_events.shrink_to_fit();
    data.bga_events.shrink_to_fit();

    // 3. この後、SoundManager::loadSoundsInBulk() を呼ぶことで、
    // きれいになったヒープ領域に音声データを詰め込むことができる
    return data;
}

BMSHeader BmsonLoader::loadHeader(const std::string& path) {
    BMSHeader h;
    std::ifstream f(path);
    if (!f.is_open()) return h;
    nlohmann::json j;
    try {
        f >> j;
        const nlohmann::json& info = (j.contains("info") && !j["info"].is_null()) ? j["info"] : j;
        
        h.title = get_string_safe(info, "title", "Unknown");
        h.artist = get_string_safe(info, "artist", "Unknown");
        h.genre = get_string_safe(info, "genre", "Unknown");
        h.modeHint = get_string_safe(info, "mode_hint", "");
        h.subtitle = get_string_safe(info, "subtitle", "");

        std::string cn = get_string_safe(info, "chart_name", "");
        int diffVal = (int)get_double_safe(info, "difficulty", -1.0);

        if (!cn.empty()) {
            std::string s = cn;
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            if (s.find("BEGINNER") != std::string::npos)      diffVal = 1;
            else if (s.find("NORMAL") != std::string::npos)   diffVal = 2;
            else if (s.find("HYPER") != std::string::npos)    diffVal = 3;
            else if (s.find("ANOTHER") != std::string::npos)  diffVal = 4;
            else if (s.find("INSANE") != std::string::npos || 
                     s.find("LEGENDARIA") != std::string::npos || 
                     s.find("LEGGENDARIA") != std::string::npos) diffVal = 5;
        }
        if (diffVal == -1) diffVal = 2;

        switch(diffVal) {
            case 1: h.chartName = "BEGINNER"; break;
            case 2: h.chartName = "NORMAL";   break;
            case 3: h.chartName = "HYPER";    break;
            case 4: h.chartName = "ANOTHER";  break;
            case 5: h.chartName = "INSANE";   break;
            default: h.chartName = "NORMAL";  break;
        }

        h.level = (int)get_double_safe(info, "level", 0.0);
        h.total = get_double_safe(info, "total", 100.0);
        h.judgeRank = get_double_safe(info, "judge_rank", 100.0);
        h.eyecatch = get_string_safe(info, "eyecatch_image", "");
        h.banner = get_string_safe(info, "banner_image", "");
        h.preview = get_string_safe(info, "preview_music", "");

        double bpm = get_double_safe(info, "bpm", -1.0);
        if (bpm <= 0) bpm = get_double_safe(info, "init_bpm", -1.0);
        if (bpm <= 0) bpm = get_double_safe(j, "bpm", -1.0);
        if (bpm <= 0) bpm = get_double_safe(j, "init_bpm", 120.0);
        
        h.bpm = bpm;
        h.min_bpm = h.bpm;
        h.max_bpm = h.bpm;

        const nlohmann::json& bpm_src = j.contains("bpm_events") ? j["bpm_events"] : (info.contains("bpm_events") ? info["bpm_events"] : nlohmann::json());
        if (bpm_src.is_array()) {
            for (auto& e : bpm_src) {
                double b = get_double_safe(e, "bpm", h.bpm);
                if (b < h.min_bpm) h.min_bpm = b;
                if (b > h.max_bpm) h.max_bpm = b;
            }
        }

        int totalNotesCount = 0;
        bool hasP1_6or7 = false;
        bool hasP2Side = false;

        const nlohmann::json& sc_src = j.contains("sound_channels") ? j["sound_channels"] : (info.contains("sound_channels") ? info["sound_channels"] : nlohmann::json());
        if (sc_src.is_array()) {
            for (auto& ch : sc_src) {
                if (ch.contains("notes") && ch["notes"].is_array()) {
                    for (auto& n : ch["notes"]) {
                        int64_t x = n.value("x", (int64_t)0);
                        if (x == 6 || x == 7) hasP1_6or7 = true;
                        if (x >= 9 && x <= 16) hasP2Side = true;

                        if (isPlayableLaneSP(x)) {
                            totalNotesCount++;
                        }
                    }
                }
            }
        }
        h.totalNotes = totalNotesCount;
        h.total = (double)totalNotesCount; 
        h.is7Key = (hasP1_6or7 && !hasP2Side);

        // --- 動画パスと開始タイミング(offset)を取得 ---
        int videoId = -1;
        if (j.contains("bga") && j["bga"].is_object()) {
            const auto& bga_node = j["bga"];
            if (bga_node.contains("bga_header") && bga_node["bga_header"].is_array()) {
                for (auto& img : bga_node["bga_header"]) {
                    std::string name = get_string_safe(img, "name", "");
                    int id = img.value("id", 0);

                    std::string low = name;
                    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                    if (low.find(".wmv") != std::string::npos || 
                        low.find(".mp4") != std::string::npos || 
                        low.find(".mpg") != std::string::npos || 
                        low.find(".mpeg") != std::string::npos || 
                        low.find(".avi") != std::string::npos ||
                        low.find(".mov") != std::string::npos ||
                        low.find(".bga") != std::string::npos) {
                        
                        h.bga_video = name;
                        videoId = id; // 動画IDを保持
                        break; 
                    }
                }
            }
            
            // loadHeaderでも動画タイミング(offset)だけは取得する
            if (videoId != -1 && bga_node.contains("bga_events") && bga_node["bga_events"].is_array()) {
                for (auto& e : bga_node["bga_events"]) {
                    int id = e.value("id", 0);
                    if (id == videoId) {
                        h.bga_offset = e.value("y", (int64_t)0);
                        break; 
                    }
                }
            }
        }

    } catch (...) {}
    return h;
}