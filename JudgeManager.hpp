#ifndef JUDGEMANAGER_HPP
#define JUDGEMANAGER_HPP

#include "CommonTypes.hpp"
#include "Config.hpp"
#include <cmath>

class JudgeManager {
public:
    void updateGauge(PlayStatus& status, int judgeType, bool isHit, double baseRecoveryPerNote);

    // ★修正：JudgeUI の label を std::string から JudgeKind に変更。
    // 呼び出し側で毎回 string をヒープ確保していた問題を解消。
    struct JudgeUI {
        JudgeKind kind;
        // color は JudgeKind から導出できるため削除（judgeKindToColor を使う）
    };
    JudgeUI getJudgeUIData(int judgeType);
};

#endif // JUDGEMANAGER_HPP




