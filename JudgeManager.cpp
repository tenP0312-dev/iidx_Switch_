#include "JudgeManager.hpp"
#include <cmath>
#include <algorithm>

void JudgeManager::updateGauge(PlayStatus& status, int judgeType, bool isHit, double baseRecoveryPerNote) {
    int currentInternal = (int)std::round(status.gauge * 50.0);
    int delta = 0;
    int opt = Config::GAUGE_OPTION;

    if (opt == 6) { // HAZARD
        if (!isHit || judgeType == 0) {
            status.gauge = 0.0; status.isFailed = true; status.isDead = true; return;
        } else delta = 8;
    }
    else if (opt >= 3) { // 減少型
        if (judgeType >= 2) delta = 8;
        else if (judgeType == 1) delta = (opt == 5) ? 2 : 0;
        else {
            if (opt == 3) delta = isHit ? -250 : -450;
            else if (opt == 4) delta = isHit ? -500 : -900;
            else if (opt == 5) delta = isHit ? -75 : -125;
            if ((opt == 3 || opt == 5) && currentInternal <= 1500) delta /= 2;
        }
    } else { // 回復型 (NORMAL: 0, EASY: 1, ASSIST: 2)
        int notes = status.totalNotes;
        double calcRecovery = 0.0;
        if (notes > 0) {
            if (notes < 350) {
                calcRecovery = 80000.0 / (notes * 6.0);
            } else {
                calcRecovery = 80000.0 / (((notes - 350) / 3.0 + 350.0) * 6.0);
            }
        }

        double recoveryScale = 1.0;

        if (judgeType >= 2)      delta = (int)(calcRecovery * recoveryScale);
        else if (judgeType == 1) delta = (int)((calcRecovery / 2.0) * recoveryScale);
        else {
            delta = isHit ? -100 : -300;
            if (opt >= 1) delta = (int)(delta * 0.8);
        }
    }

    currentInternal += delta;
    if (currentInternal > 5000) currentInternal = 5000;
    if (opt >= 3 || opt == 6) {
        if (currentInternal <= 0) {
            status.gauge = 0.0; status.isFailed = true; status.isDead = true;
            status.clearType = ClearType::FAILED;
            return;
        }
    } else {
        if (currentInternal < 100) currentInternal = 100;
    }
    status.gauge = (double)currentInternal / 50.0;
}

// ★修正：std::string label を廃止し JudgeKind を返す。
// 呼び出し元が currentJudge = {"P-GREAT", ...} などと string を構築していた
// ヒープアロケーションを根絶する。
JudgeManager::JudgeUI JudgeManager::getJudgeUIData(int judgeType) {
    return { judgeTypeToKind(judgeType) };
}




