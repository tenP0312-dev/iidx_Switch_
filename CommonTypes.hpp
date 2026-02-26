#ifndef COMMONTYPES_HPP
#define COMMONTYPES_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <SDL2/SDL.h>

/**
 * シーンからの戻り値を定義
 */
enum class SelectSceneResult {
    CONTINUE,           // 継続
    PLAY_SONG,          // 曲決定
    BACK_TO_MODE_SELECT,// モードセレクトへ戻る
    QUIT_GAME           // ゲーム終了
};

/**
 * クリア状態の定義
 */
enum class ClearType : int {
    NO_PLAY = 0,
    FAILED = 1,
    DAN_CLEAR = 2,
    ASSIST_CLEAR = 3,
    EASY_CLEAR = 4,
    NORMAL_CLEAR = 5,
    HARD_CLEAR = 6,
    EX_HARD_CLEAR = 7,
    FULL_COMBO = 8
};

/**
 * ノーツ情報
 */
struct PlayableNote { 
    double target_ms; 
    int64_t y;
    int lane; 
    std::string channel_name; 
    bool played = false; 
    bool isBGM = false; 

    // ロングノーツ(LN)用
    bool isLN = false;
    int64_t l = 0;
    double duration_ms = 0;
    bool isBeingPressed = false;
    bool end_processed = false;
};

/**
 * 小節線
 */
struct PlayableLine { 
    double target_ms; 
    int64_t y;
};

/**
 * 判定エフェクト
 */
struct ActiveEffect { 
    int lane; 
    uint32_t startTime; 
};

/**
 * 判定表示用
 */
struct JudgmentDisplay { 
    std::string text; 
    uint32_t startTime; 
    bool active = false; 
    SDL_Color color = {255, 255, 255, 255}; 

    bool isFast = false;
    bool isSlow = false;

    JudgmentDisplay() : text(""), startTime(0), active(false), isFast(false), isSlow(false) {}

    JudgmentDisplay(std::string t, uint32_t s, bool a, SDL_Color c, bool f = false, bool sl = false)
        : text(t), startTime(s), active(a), color(c), isFast(f), isSlow(sl) {}
};

/**
 * プレイ状況
 */
struct PlayStatus {
    int pGreatCount = 0;
    int greatCount = 0;
    int goodCount = 0;
    int badCount = 0;
    int poorCount = 0;

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

    ClearType clearType = ClearType::NO_PLAY;
    bool isDead = false;

    std::vector<float> gaugeHistory; 
};

/**
 * ベストスコア記録用
 */
struct BestScore {
    int pGreat = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int poor = 0;

    int fastCount = 0;
    int slowCount = 0;

    int maxCombo = 0;
    int exScore = 0;
    std::string rank = "F";
    ClearType clearType = ClearType::NO_PLAY;
    bool isClear = false;

    std::vector<float> gaugeHistory;
};

#endif