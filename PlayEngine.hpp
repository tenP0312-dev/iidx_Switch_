#ifndef PLAYENGINE_HPP
#define PLAYENGINE_HPP

#include <vector>
#include <string>
#include <SDL2/SDL.h>
#include "CommonTypes.hpp"
#include "BMSData.hpp"
#include "SoundManager.hpp"
#include "ChartProjector.hpp"
#include "JudgeManager.hpp"

class PlayEngine {
public:
    void init(const BMSData& data);
    void update(double cur_ms, uint32_t now, SoundManager& snd);
    int processHit(int lane, double cur_ms, uint32_t now, SoundManager& snd);
    void processRelease(int lane, double cur_ms, uint32_t now);
    void forceFail();

    double getMsFromY(int64_t target_y) const;
    int64_t getYFromMs(double cur_ms) const;
    double getBpmFromMs(double cur_ms) const;

    PlayStatus& getStatus() { return status; }
    const std::vector<PlayableNote>& getNotes() const { return notes; }
    const std::vector<PlayableLine>& getBeatLines() const { return beatLines; }
    JudgmentDisplay& getCurrentJudge() { return currentJudge; }

private:
    BMSData bmsData;
    std::vector<PlayableNote> notes;
    std::vector<PlayableLine> beatLines;
    PlayStatus status;
    JudgmentDisplay currentJudge;

    ChartProjector projector;
    JudgeManager judgeManager;

    double baseRecoveryPerNote = 0.0;
    size_t nextUpdateIndex = 0;
    std::string lastSoundPerLane[9];
    double lastHistoryUpdateMs = -1000.0;
};

#endif