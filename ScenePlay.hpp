#ifndef SCENEPLAY_HPP
#define SCENEPLAY_HPP

#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "SoundManager.hpp"
#include "NoteRenderer.hpp"
#include "CommonTypes.hpp"
#include "BMSData.hpp"

// ボムの独立したアニメーション管理用
struct BombAnim {
    int lane;
    uint32_t startTime;
    int judgeType; // 【追加】0:なし, 1:P-GREAT, 2:GREAT, 3:その他
};

// 前方宣言
class PlayEngine;
class BgaManager; 

class ScenePlay {
public:
    bool run(SDL_Renderer* ren, SoundManager& snd, NoteRenderer& renderer, const std::string& bmsonPath);
    const PlayStatus& getStatus() const { return status; }
    const BMSHeader& getHeader() const { return currentHeader; }

private:
    bool processInput(double cur_ms, uint32_t now, SoundManager& snd, PlayEngine& engine);
    bool isAutoLane(int lane);
    void updateAssist(double cur_ms, PlayEngine& engine, SoundManager& snd);
    int getLaneFromJoystickButton(int btn);
    
    void renderScene(SDL_Renderer* ren, NoteRenderer& renderer, PlayEngine& engine, 
                     BgaManager& bga, 
                     double cur_ms, int64_t cur_y, int fps, const BMSHeader& header, 
                     uint32_t now, double progress);

    std::vector<ActiveEffect> effects;  
    std::vector<BombAnim> bombAnims; 
    PlayStatus status;          
    BMSHeader currentHeader;

    bool isAssistUsed = false;
    bool startButtonPressed = false;     
    bool effectButtonPressed = false;    

    bool lanePressed[9] = {false}; 

    bool scratchUpActive = false;
    bool scratchDownActive = false;

    uint32_t lastStartPressTime = 0;
    int backupSudden = 300; 
};

#endif