#include "JudgeManager.hpp"
#include <cmath>
#include <algorithm>

void JudgeManager::updateGauge(PlayStatus& status, int judgeType, bool isHit, double baseRecoveryPerNote) {
    int currentInternal = std::round(status.gauge * 50.0);
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
    } else { // 回復型 (NORMAL: 0, EASY: 1, LIGHT: 2)
        // --- 【追加】ご提示の計算式に基づく回復率の算出 ---
        int notes = status.totalNotes;
        double calcRecovery = 0.0;
        if (notes > 0) {
            if (notes < 350) {
                calcRecovery = 80000.0 / (notes * 6.0);
            } else {
                calcRecovery = 80000.0 / (((notes - 350) / 3.0 + 350.0) * 6.0);
            }
        }
        // ------------------------------------------------

        double recoveryScale = 1.0; 
        
        if (judgeType >= 2) delta = (int)(calcRecovery * recoveryScale);
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
            status.gauge = 0.0; status.isFailed = true; status.isDead = true; status.clearType = ClearType::FAILED;
            return;
        }
    } else {
        if (currentInternal < 100) currentInternal = 100;
    }
    status.gauge = (double)currentInternal / 50.0;
}

JudgeManager::JudgeUI JudgeManager::getJudgeUIData(int judgeType) {
    if (judgeType == 3) return {"P-GREAT", {255, 255, 255, 255}};
    if (judgeType == 2) return {"GREAT", {255, 255, 0, 255}};
    if (judgeType == 1) return {"GOOD", {255, 255, 0, 255}};
    if (judgeType == 0) return {"BAD", {255, 128, 0, 255}};
    return {"POOR", {255, 128, 0, 255}};
}