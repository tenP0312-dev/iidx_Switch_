#ifndef COMMONTYPES_HPP
#define COMMONTYPES_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <SDL2/SDL.h>

/**
 * 【追加】シーンからの戻り値を定義
 * 選曲画面から main への命令を整理するために使用します。
 */
enum class SelectSceneResult {
    CONTINUE,           // 継続（何もなし）
    PLAY_SONG,          // 曲決定（プレイ開始）
    BACK_TO_MODE_SELECT,// モードセレクトへ戻る
    QUIT_GAME           // ゲーム終了（アプリ終了）
};

/**
 * クリア状態の定義 (数値が大きいほど上位の記録として扱う)
 * 既存の順序と定義を完全に維持します。
 */
enum class ClearType : int {
    NO_PLAY = 0,      // 未プレイ
    FAILED = 1,       // 途中終了（閉店）
    DAN_CLEAR = 2,    // 段位ゲージクリア
    ASSIST_CLEAR = 3, // アシストオプション使用、または A-EASY クリア
    EASY_CLEAR = 4,   // イージーゲージクリア
    NORMAL_CLEAR = 5, // ノーマルゲージクリア
    HARD_CLEAR = 6,   // ハードゲージクリア
    EX_HARD_CLEAR = 7,// EXハードクリア
    FULL_COMBO = 8    // フルコンボ
};

/**
 * ノーツの情報を保持する構造体 (100%継承 + LN対応拡張)
 */
struct PlayableNote { 
    double target_ms; 
    int64_t y;
    int lane; 
    std::string channel_name; 
    bool played = false; 
    bool isBGM = false; 

    // --- 【追加】ロングノーツ(CN/BSS)対応フィールド ---
    bool isLN = false;          // これがLN(長さを持つノーツ)ならtrue
    int64_t l = 0;              // bmson上の長さ(pulse)
    double duration_ms = 0;     // ミリ秒単位の長さ
    bool isBeingPressed = false;// 現在「押しっぱなし/回しっぱなし」の最中か
    bool end_processed = false; // 終点(離し/停止)の判定まで完了したか
};

/**
 * 小節線の情報を保持する構造体 (100%継承)
 */
struct PlayableLine { 
    double target_ms; 
    int64_t y;
};

/**
 * 判定エフェクト（爆発）の情報を保持する構造体 (100%継承)
 */
struct ActiveEffect { 
    int lane; 
    uint32_t startTime; 
};

/**
 * 画面に表示する判定文字（PERFECT等）の情報 (100%継承)
 * --- コンストラクタを追加し、PlayEngineでの一括代入に対応 ---
 */
struct JudgmentDisplay { 
    std::string text; 
    uint32_t startTime; 
    bool active = false; 
    SDL_Color color = {255, 255, 255, 255}; 

    // --- 追加: 判定表示用のFAST/SLOWフラグ ---
    bool isFast = false;
    bool isSlow = false;

    // デフォルトコンストラクタ
    JudgmentDisplay() : text(""), startTime(0), active(false), isFast(false), isSlow(false) {}

    // PlayEngine.cpp の {...} 形式の代入を可能にするコンストラクタ
    JudgmentDisplay(std::string t, uint32_t s, bool a, SDL_Color c, bool f = false, bool sl = false)
        : text(t), startTime(s), active(a), color(c), isFast(f), isSlow(sl) {}
};

/**
 * プレイ中の統計情報およびゲージ状況
 * 既存ロジックを維持しつつ、精密な判定・描画に必要なフィールドを追加
 */
struct PlayStatus {
    int pGreatCount = 0;
    int greatCount = 0;
    int goodCount = 0;
    int badCount = 0;
    int poorCount = 0;

    // --- 追加: FAST/SLOW カウント用 ---
    int fastCount = 0;
    int slowCount = 0;

    int combo = 0;
    int maxCombo = 0;
    int totalNotes = 0;
    int remainingNotes = 0;
    double currentBpm = 0;
    double maxTargetMs = 0;
    double gauge = 22.0;
    bool isFailed = false;
    int exScore = 0;

    // --- 【追加済】精密なゲージ判定とクリアランプ決定のために必要最小限のフィールド ---
    ClearType clearType = ClearType::NO_PLAY; // プレイ中の暫定クリアタイプ
    bool isDead = false;                     // 減少型ゲージで0%になった瞬間の死亡フラグ

    // --- 【追加】リザルト画面でのグラフ描画用 ---
    std::vector<float> gaugeHistory; 
};

/**
 * 楽曲ごとの自己ベスト記録を保持する構造体 (セーブデータ用) (100%継承)
 */
struct BestScore {
    int pGreat = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int poor = 0;

    // --- 追加: 自己ベストのFAST/SLOW保存用 ---
    int fastCount = 0;
    int slowCount = 0;

    int maxCombo = 0;
    int exScore = 0;      // P-GREAT*2 + GREAT*1
    std::string rank = "F";
    ClearType clearType = ClearType::NO_PLAY; // クリアランプの状態
    bool isClear = false; // 完走したかどうかのフラグ (clearTypeがDAN以上ならtrue)

    // --- 【追加】自己ベスト時のゲージ推移グラフデータ ---
    std::vector<float> gaugeHistory;
};

#endif