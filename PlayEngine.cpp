#include "PlayEngine.hpp"
#include "Config.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <map>
#include <set>

// TOTAL値計算 (HappySky仕様)
int calculateHSRecoveryInternal(int notes) {
    if (notes <= 0) return 0;
    if (notes < 350) {
        return 80000 / (notes * 6);
    } else {
        return 80000 / (notes * 2 + 1400);
    }
}

void PlayEngine::init(const BMSData& data) {
    bmsData = data;
    projector.init(bmsData);

    status = PlayStatus();
    notes.clear();
    notes.reserve(data.header.totalNotes + 100);
    beatLines.clear();
    currentJudge = JudgmentDisplay();

    status.totalNotes   = 0;
    status.maxTargetMs  = 0;

    int laneMap[9];
    for (int i = 0; i <= 8; i++) laneMap[i] = i;

    std::random_device rd;
    std::mt19937 g(rd());

    if (Config::PLAY_OPTION == 1) { // RANDOM
        std::vector<int> kbd = {1, 2, 3, 4, 5, 6, 7};
        std::shuffle(kbd.begin(), kbd.end(), g);
        for (int i = 1; i <= 7; i++) laneMap[i] = kbd[i - 1];
    }
    else if (Config::PLAY_OPTION == 2) { // R-RANDOM
        int shift = std::uniform_int_distribution<int>(1, 6)(g);
        for (int i = 1; i <= 7; i++) laneMap[i] = ((i - 1 + shift) % 7) + 1;
    }
    else if (Config::PLAY_OPTION == 4) { // MIRROR
        for (int i = 1; i <= 7; i++) laneMap[i] = 8 - i;
    }

    struct TempNote {
        int64_t  y;
        int64_t  l;
        int      originalLane;
        uint32_t soundId;
        bool     isBGM;
    };
    std::vector<TempNote> tempNotes;
    tempNotes.reserve(data.header.totalNotes * 2);

    for (const auto& ch : bmsData.sound_channels) {
        uint32_t sId = std::hash<std::string>{}(ch.name);
        for (const auto& n : ch.notes) {
            bool isBGM = (n.x < 1 || n.x > 8);
            tempNotes.push_back({n.y, n.l, (int)n.x, sId, isBGM});
        }
    }

    std::sort(tempNotes.begin(), tempNotes.end(), [](const TempNote& a, const TempNote& b) {
        if (a.y != b.y) return a.y < b.y;
        return a.originalLane < b.originalLane;
    });

    std::map<int64_t, std::set<int>> usedLanesAtY;

    for (const auto& tn : tempNotes) {
        PlayableNote pn;
        pn.target_ms = projector.getMsFromY(tn.y);
        pn.y         = tn.y;
        pn.soundId   = tn.soundId;
        pn.isBGM     = tn.isBGM;

        bool isLegacyModel = (Config::ASSIST_OPTION == 2 || Config::ASSIST_OPTION == 4 || Config::ASSIST_OPTION == 6);

        pn.l = tn.l;
        if (pn.l > 0) {
            if (isLegacyModel) {
                pn.l         = 0;
                pn.isLN      = false;
                pn.duration_ms = 0;
            } else {
                pn.isLN      = true;
                double end_ms  = projector.getMsFromY(tn.y + tn.l);
                pn.duration_ms = end_ms - pn.target_ms;
            }
        } else {
            pn.isLN      = false;
            pn.duration_ms = 0;
        }

        if (!pn.isBGM) {
            status.totalNotes++;
            if (tn.originalLane == 8) {
                pn.lane = 8;
            } else {
                if (Config::PLAY_OPTION == 3) { // S-RANDOM
                    std::vector<int> candidates = {1, 2, 3, 4, 5, 6, 7};
                    std::shuffle(candidates.begin(), candidates.end(), g);
                    int selected = candidates[0];
                    for (int c : candidates) {
                        if (usedLanesAtY[tn.y].find(c) == usedLanesAtY[tn.y].end()) {
                            selected = c;
                            break;
                        }
                    }
                    pn.lane = selected;
                    usedLanesAtY[tn.y].insert(selected);
                } else {
                    pn.lane = laneMap[tn.originalLane];
                }
            }
        } else {
            pn.lane = tn.originalLane;
        }

        notes.push_back(pn);

        double noteEndMs = pn.target_ms + pn.duration_ms;
        if (noteEndMs > status.maxTargetMs) status.maxTargetMs = noteEndMs;
    }

    std::sort(notes.begin(), notes.end(), [](const PlayableNote& a, const PlayableNote& b) {
        return a.target_ms < b.target_ms;
    });

    status.remainingNotes = status.totalNotes;
    for (const auto& l : bmsData.lines) beatLines.push_back({projector.getMsFromY(l.y), l.y});

    baseRecoveryPerNote = (double)calculateHSRecoveryInternal(status.totalNotes);

    if (Config::GAUGE_OPTION == 5) status.gauge = (double)Config::DAN_GAUGE_START_PERCENT;
    else if (Config::GAUGE_OPTION >= 3) status.gauge = 100.0;
    else status.gauge = 22.0;

    status.isFailed  = false;
    status.isDead    = false;
    status.clearType = ClearType::NO_PLAY;

    nextUpdateIndex = 0;
    for (int i = 0; i <= 8; i++) lastSoundPerLaneId[i] = 0;

    // ★修正：gaugeHistory を事前確保して push_back 時の再アロケーションを防ぐ
    status.gaugeHistory.clear();
    status.gaugeHistory.reserve(2000); // 最大曲長(~6分) x 200ms間隔 = 1800サンプル程度
    lastHistoryUpdateMs = -1000.0;
}

void PlayEngine::update(double cur_ms, uint32_t now, SoundManager& snd) {
    if (status.isFailed) return;

    if (cur_ms >= 0 && cur_ms - lastHistoryUpdateMs >= 200.0) {
        status.gaugeHistory.push_back((float)status.gauge);
        lastHistoryUpdateMs = cur_ms;
    }

    for (size_t i = nextUpdateIndex; i < notes.size(); ++i) {
        auto& n = notes[i];

        if (n.played) {
            if (i == nextUpdateIndex) nextUpdateIndex++;
            continue;
        }

        if (n.target_ms > cur_ms + 500.0) break;

        if (!n.isBGM) {
            lastSoundPerLaneId[n.lane] = n.soundId;
        }

        if (n.isBGM && n.target_ms <= cur_ms) {
            snd.play(n.soundId);
            n.played = true;
            if (i == nextUpdateIndex) nextUpdateIndex++;
        }
        else if (!n.isBGM) {
            double adjusted_target = n.target_ms + Config::JUDGE_OFFSET;
            double end_ms = adjusted_target + (n.isLN ? n.duration_ms : 0);

            if (cur_ms > end_ms + Config::JUDGE_POOR) {
                n.played        = true;
                n.isBeingPressed = false;
                status.remainingNotes--;
                status.poorCount++;
                status.combo = 0;

                // ★修正：string 代入なし。enum を直接セット
                currentJudge.kind      = JudgeKind::POOR;
                currentJudge.startTime = now;
                currentJudge.active    = true;
                currentJudge.isFast    = false;
                currentJudge.isSlow    = false;

                judgeManager.updateGauge(status, 0, false, baseRecoveryPerNote);

                if (i == nextUpdateIndex) nextUpdateIndex++;

                if (status.isFailed) {
                    snd.stopAll();
                    break;
                }
            }
        }
    }

    if (cur_ms > status.maxTargetMs + 1000.0) {
        if (!status.isFailed) {
            int opt = Config::GAUGE_OPTION;
            if (opt >= 3) {
                if (status.badCount == 0 && status.poorCount == 0) status.clearType = ClearType::FULL_COMBO;
                else if (opt == 3) status.clearType = ClearType::HARD_CLEAR;
                else if (opt == 4) status.clearType = ClearType::EX_HARD_CLEAR;
                else if (opt == 5) status.clearType = ClearType::DAN_CLEAR;
                else if (opt == 6) status.clearType = ClearType::FULL_COMBO;
            }
            else {
                double border = (opt == 1) ? 60.0 : 80.0;
                if (status.gauge >= border) {
                    if (status.badCount == 0 && status.poorCount == 0) status.clearType = ClearType::FULL_COMBO;
                    else if (opt == 1) status.clearType = ClearType::ASSIST_CLEAR;
                    else if (opt == 2) status.clearType = ClearType::EASY_CLEAR;
                    else               status.clearType = ClearType::NORMAL_CLEAR;
                } else {
                    status.isFailed  = true;
                    status.clearType = ClearType::FAILED;
                }
            }
        }
    }
}

int PlayEngine::processHit(int lane, double cur_ms, uint32_t now, SoundManager& snd) {
    if (status.isFailed) return 0;

    bool hitSuccess = false;
    int  finalJudge = 0;

    for (size_t i = nextUpdateIndex; i < notes.size(); ++i) {
        auto& n = notes[i];
        if (n.played || n.isBGM || n.lane != lane) continue;
        if (n.isLN && n.isBeingPressed) continue;

        double adjusted_target = n.target_ms + Config::JUDGE_OFFSET;
        double raw_diff        = cur_ms - adjusted_target;
        double diff            = std::abs(raw_diff);

        if (diff > Config::JUDGE_BAD) {
            if (adjusted_target > cur_ms + Config::JUDGE_BAD) break;
            continue;
        }

        snd.play(n.soundId);
        lastSoundPerLaneId[lane] = n.soundId;

        if (n.isLN) {
            n.isBeingPressed = true;
        } else {
            n.played = true;
            status.remainingNotes--;
        }

        hitSuccess = true;

        int  judgeType = 0;
        bool isFast    = (raw_diff < 0);
        bool isSlow    = (raw_diff > 0);

        if (diff <= Config::JUDGE_PGREAT) {
            status.pGreatCount++; status.combo++; judgeType = 3;
            status.exScore += 2;
            isFast = false; isSlow = false;
        } else if (diff <= Config::JUDGE_GREAT) {
            status.greatCount++; status.combo++; judgeType = 2;
            status.exScore += 1;
            if (isFast) status.fastCount++; else status.slowCount++;
        } else if (diff <= Config::JUDGE_GOOD) {
            status.goodCount++; status.combo++; judgeType = 1;
            if (isFast) status.fastCount++; else status.slowCount++;
        } else {
            status.badCount++;
            status.combo = 0;
            judgeType = 0;
            if (n.isLN) {
                n.isBeingPressed = false;
                n.played         = true;
                status.remainingNotes--;
            }
            isFast = false; isSlow = false;
        }

        finalJudge = judgeType;

        // ★修正：JudgeUI.kind を使い、string 代入を完全に排除
        auto uiData = judgeManager.getJudgeUIData(judgeType);
        currentJudge.kind      = uiData.kind;
        currentJudge.startTime = now;
        currentJudge.active    = true;
        currentJudge.isFast    = isFast;
        currentJudge.isSlow    = isSlow;

        if (status.combo > status.maxCombo) status.maxCombo = status.combo;
        judgeManager.updateGauge(status, judgeType, true, baseRecoveryPerNote);

        if (status.isFailed) snd.stopAll();
        break;
    }

    if (!hitSuccess) {
        if (lane >= 1 && lane <= 8 && lastSoundPerLaneId[lane] != 0) {
            snd.play(lastSoundPerLaneId[lane]);
            if (Config::GAUGE_OPTION == 6) { // HAZARD
                status.gauge     = 0.0;
                status.isFailed  = true;
                status.isDead    = true;
                snd.stopAll();
            }
        }
    }

    return finalJudge;
}

void PlayEngine::processRelease(int lane, double cur_ms, uint32_t now) {
    if (status.isFailed) return;

    for (size_t i = nextUpdateIndex; i < notes.size(); ++i) {
        auto& n = notes[i];

        if (n.played) continue;
        if (n.target_ms > cur_ms + 1000.0) break;

        if (n.isLN && n.lane == lane && n.isBeingPressed) {
            double adjusted_end = (n.target_ms + n.duration_ms) + Config::JUDGE_OFFSET;
            double raw_diff     = cur_ms - adjusted_end;
            double diff         = std::abs(raw_diff);

            if (diff <= Config::JUDGE_BAD) {
                n.isBeingPressed = false;
                n.played         = true;
                status.remainingNotes--;

                int  judgeType = 0;
                bool isFast    = (raw_diff < 0);
                bool isSlow    = (raw_diff > 0);

                if (diff <= Config::JUDGE_PGREAT) {
                    status.pGreatCount++; status.combo++; judgeType = 3;
                    status.exScore += 2;
                    isFast = false; isSlow = false;
                } else if (diff <= Config::JUDGE_GREAT) {
                    status.greatCount++; status.combo++; judgeType = 2;
                    status.exScore += 1;
                    if (isFast) status.fastCount++; else status.slowCount++;
                } else if (diff <= Config::JUDGE_GOOD) {
                    status.goodCount++; status.combo++; judgeType = 1;
                    if (isFast) status.fastCount++; else status.slowCount++;
                } else {
                    status.badCount++; status.combo = 0; judgeType = 0;
                    isFast = false; isSlow = false;
                }

                auto uiData = judgeManager.getJudgeUIData(judgeType);
                currentJudge.kind      = uiData.kind;
                currentJudge.startTime = now;
                currentJudge.active    = true;
                currentJudge.isFast    = isFast;
                currentJudge.isSlow    = isSlow;

                if (status.combo > status.maxCombo) status.maxCombo = status.combo;
                judgeManager.updateGauge(status, judgeType, true, baseRecoveryPerNote);
            } else {
                n.isBeingPressed = false;
                n.played         = true;
                status.remainingNotes--;
                status.poorCount++;
                status.combo = 0;

                currentJudge.kind      = JudgeKind::POOR;
                currentJudge.startTime = now;
                currentJudge.active    = true;
                currentJudge.isFast    = false;
                currentJudge.isSlow    = false;

                judgeManager.updateGauge(status, 0, false, baseRecoveryPerNote);
            }
            break;
        }
    }
}

double PlayEngine::getMsFromY(int64_t target_y) const { return projector.getMsFromY(target_y); }
int64_t PlayEngine::getYFromMs(double cur_ms) const   { return projector.getYFromMs(cur_ms); }
double PlayEngine::getBpmFromMs(double cur_ms) const  { return projector.getBpmFromMs(cur_ms); }

void PlayEngine::forceFail() {
    status.isFailed  = true;
    status.isDead    = true;
    status.gauge     = 0.0;
    status.clearType = ClearType::FAILED;
}




