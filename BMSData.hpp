#ifndef BMSDATA_HPP
#define BMSDATA_HPP

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include "BgaManager.hpp" // BgaEvent構造体のために追加

struct BMSNote {
    int64_t x, y, l;
};

struct BMSLine {
    int64_t y;
};

struct BMSSoundChannel {
    std::string name;
    std::vector<BMSNote> notes;
};

struct BPMEvent {
    int64_t y;
    double bpm;
};

struct BMSHeader {
    std::string title, artist, genre;
    std::string modeHint; // bmsonの "mode_hint" (beat-7k, beat-14k等)
    double bpm;
    int resolution;

    // --- 既存ロジック・ビルド互換性維持のための変数 ---
    double min_bpm;   // 最小BPM
    double max_bpm;   // 最大BPM
    int totalNotes;   // 総ノーツ数

    // --- 【追加】将来的な機能拡張（bmson情報の保持）用の変数 ---
    std::string subtitle;     // サブタイトル
    std::string chartName;    // 難易度名 (HYPER, ANOTHER等)
    int level;                // 【修正：追加】難易度数値
    double total;             // TOTAL値 (ゲージ増分用)
    double judgeRank;         // 判定ランク
    std::string eyecatch;     // アイキャッチ画像パス
    std::string banner;       // バナー画像パス
    std::string preview;      // プレビュー音声パス

    // --- 判定用フラグ ---
    bool is7Key;              // 【追加】7鍵盤（SP）譜面かどうかの判定用フラグ

    // --- 【追加：動画BGA対応】 ---
    std::string bga_video;    // 動画ファイルパス (.bga)
};

class BMSData {
public:
    BMSHeader header;
    std::vector<BMSSoundChannel> sound_channels;
    std::vector<BMSLine> lines;
    std::vector<BPMEvent> bpm_events; 

    // --- BGA機能のための最小限の追加 ---
    std::unordered_map<int, std::string> bga_images; // IDと画像ファイル名の対応
    std::vector<BgaEvent> bga_events;                // タイムライン上のBGAイベント
    std::vector<BgaEvent> layer_events;              // 【追加】前面レイヤー
    std::vector<BgaEvent> poor_events;               // 【追加】ミス画像
};

#endif