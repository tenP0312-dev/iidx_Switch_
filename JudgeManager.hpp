#ifndef JUDGEMANAGER_HPP
#define JUDGEMANAGER_HPP

#include "CommonTypes.hpp"
#include "Config.hpp"
#include <cmath>
#include <string>

class JudgeManager {
public:
    // 既存の updateGauge ロジックをそのまま移植
    void updateGauge(PlayStatus& status, int judgeType, bool isHit, double baseRecoveryPerNote);

    // 判定用文字列と色の構造体 (デザイン維持用)
    struct JudgeUI {
        std::string label;
        SDL_Color color;
    };
    JudgeUI getJudgeUIData(int judgeType);
};

#endif