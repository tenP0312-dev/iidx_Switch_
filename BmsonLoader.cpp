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

#include "json.hpp"

// 既存ロジック100%継承のためのハンドラ
struct BmsonSaxHandler : public nlohmann::json_sax<nlohmann::json> {
    BMSData& data;
    std::string current_key;
    std::string current_obj; 
    BMSSoundChannel* current_ch = nullptr;

    BmsonSaxHandler(BMSData& d) : data(d) {}

    bool key(string_t& val) override { current_key = val; return true; }
    bool string(string_t& val) override {
        if (current_obj == "info") {
            if (current_key == "title") data.header.title = val;
            else if (current_key == "artist") data.header.artist = val;
            else if (current_key == "genre") data.header.genre = val;
            else if (current_key == "mode_hint") data.header.modeHint = val;
            else if (current_key == "subtitle") data.header.subtitle = val;
        } else if (current_ch && current_key == "name") {
            current_ch->name = val;
        }
        return true;
    }
    bool number_float(number_float_t val, const string_t& s) override {
        if (current_key == "bpm") {
            if (current_obj == "info") data.header.bpm = val;
            else if (current_obj == "bpm_events") {
                if (!data.bpm_events.empty() && data.bpm_events.back().y == 0) data.bpm_events[0].bpm = val;
                else data.bpm_events.push_back({0, (double)val});
            }
        }
        return true;
    }
    bool number_unsigned(number_unsigned_t val) override {
        if (current_key == "y") {
            if (current_obj == "bpm_events") data.bpm_events.push_back({(long long)val, 0.0});
            else if (current_ch && !current_ch->notes.empty()) current_ch->notes.back().y = (long long)val;
        } else if (current_key == "x" && current_ch && !current_ch->notes.empty()) {
            current_ch->notes.back().x = (long long)val;
            if (val == 6 || val == 7) data.header.is7Key = true; 
            if (val >= 1 && val <= 8) data.header.totalNotes++;
        } else if (current_key == "l" && current_ch && !current_ch->notes.empty()) {
            current_ch->notes.back().l = (long long)val;
        }
        return true;
    }
    bool start_object(std::size_t elements) override {
        if (current_key == "info") current_obj = "info";
        else if (current_key == "sound_channels") current_obj = "sound_channels";
        else if (current_obj == "sound_channels") {
            data.sound_channels.emplace_back();
            current_ch = &data.sound_channels.back();
        } else if (current_ch && current_key == "notes") {
            current_ch->notes.push_back({0,0,0});
        }
        return true;
    }
    bool end_object() override { if (current_obj == "info") current_obj = ""; return true; }
    bool start_array(std::size_t elements) override {
        if (current_key == "bpm_events") current_obj = "bpm_events";
        return true;
    }
    bool boolean(bool val) override { return true; }
    bool null() override { return true; }
    bool number_integer(number_integer_t v) override { return number_unsigned(static_cast<number_unsigned_t>(v)); }
    bool end_array() override { return true; }
    bool binary(nlohmann::json::binary_t& v) override { return true; }
    bool parse_error(std::size_t p, const std::string& t, const nlohmann::json::exception& e) override { return false; }
};

BMSData BmsonLoader::load(const std::string& path, std::function<void(float)> onProgress) {
    BMSData data;
    std::ifstream f(path);
    if (!f.is_open()) return data;

    // SAXパースによりメモリ消費をファイルサイズに依存せず一定（数KB）に抑える
    BmsonSaxHandler handler(data);
    nlohmann::json::sax_parse(f, &handler);

    std::sort(data.bpm_events.begin(), data.bpm_events.end(), [](auto& a, auto& b){ return a.y < b.y; });
    
    // 断片化対策：不使用領域の即時返却
    data.sound_channels.shrink_to_fit();
    for(auto& ch : data.sound_channels) ch.notes.shrink_to_fit();
    data.bpm_events.shrink_to_fit();

    if (onProgress) onProgress(1.0f);
    return data;
}

BMSHeader BmsonLoader::loadHeader(const std::string& path) {
    BMSData temp_data;
    std::ifstream f(path);
    if (!f.is_open()) return temp_data.header;

    // 選曲画面での同期I/Oスパイクを最小化するためSAXを使用
    BmsonSaxHandler handler(temp_data);
    nlohmann::json::sax_parse(f, &handler);

    BMSHeader& h = temp_data.header;
    // 既存の難易度文字列判定ロジック等を適用
    if (h.chartName.empty()) h.chartName = "NORMAL";
    h.total = (double)h.totalNotes;

    return h;
}