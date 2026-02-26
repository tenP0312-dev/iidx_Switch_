#ifndef NOTERENDERER_HPP
#define NOTERENDERER_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <map>
#include <vector>
#include "BmsonLoader.hpp"
#include "CommonTypes.hpp"

/**
 * @brief ゲーム内の描画を専門に扱うクラス
 */
class NoteRenderer {
public:
    void init(SDL_Renderer* ren); 
    void cleanup();

    // テキストキャッシュをクリアする（メモリ解放用）
    void clearTextCache();

    void drawText(SDL_Renderer* ren, const std::string& text, int x, int y, SDL_Color color, bool isBig, bool isCenter = false, bool isRight = false, const std::string& fontPath = "");
    void drawTextCached(SDL_Renderer* ren, const std::string& text, int x, int y, SDL_Color color, bool isBig, bool isCenter = false, bool isRight = false, const std::string& fontPath = "");
    void drawImage(SDL_Renderer* ren, const std::string& path, int x, int y, int w, int h, int alpha = 255);
    void renderBackground(SDL_Renderer* ren);
    void renderLanes(SDL_Renderer* ren, double progress);
    void renderNote(SDL_Renderer* ren, const PlayableNote& note, double cur_ms, double speed, bool isAuto = false);
    void renderBeatLine(SDL_Renderer* ren, double diff, double speed);
    void renderHitEffect(SDL_Renderer* ren, int lane, float progress);
    
    void renderBomb(SDL_Renderer* ren, int lane, int frame);

    void renderJudgment(SDL_Renderer* ren, const std::string& text, float progress, SDL_Color color, int combo = 0);
    void renderCombo(SDL_Renderer* ren, int combo);
    
    void renderGauge(SDL_Renderer* ren, double gaugeValue, int gaugeOption, bool isFailed);
    void renderUI(SDL_Renderer* ren, const BMSHeader& header, int fps, double bpm, int exScore);
    void renderDecisionInfo(SDL_Renderer* ren, const BMSHeader& header);
    void renderLoading(SDL_Renderer* ren, int current, int total, const std::string& filename);
    void renderResult(SDL_Renderer* ren, const PlayStatus& status, const BMSHeader& header, const std::string& rank);

private:
    TTF_Font* fontSmall = nullptr;
    TTF_Font* fontBig = nullptr;
    std::map<std::string, SDL_Texture*> textureCache;
    std::map<std::string, TTF_Font*> customFontCache;
    std::map<std::string, SDL_Texture*> textTextureCache;

    SDL_Texture* lane_Flame = nullptr;
    SDL_Texture* lane_Flame2 = nullptr;

    SDL_Texture* texBackground = nullptr;
    SDL_Texture* texNoteWhite = nullptr;
    SDL_Texture* texNoteBlue  = nullptr;
    SDL_Texture* texNoteRed   = nullptr;
    
    SDL_Texture* texNoteWhite_LN = nullptr;
    SDL_Texture* texNoteWhite_LN_Active1 = nullptr;
    SDL_Texture* texNoteWhite_LN_Active2 = nullptr;
    SDL_Texture* texNoteBlue_LN = nullptr;
    SDL_Texture* texNoteBlue_LN_Active1 = nullptr;
    SDL_Texture* texNoteBlue_LN_Active2 = nullptr;
    SDL_Texture* texNoteRed_LN = nullptr;
    SDL_Texture* texNoteRed_LN_Active1 = nullptr;
    SDL_Texture* texNoteRed_LN_Active2 = nullptr;

    SDL_Texture* texNoteWhite_LNS = nullptr;
    SDL_Texture* texNoteWhite_LNE = nullptr;
    SDL_Texture* texNoteBlue_LNS  = nullptr;
    SDL_Texture* texNoteBlue_LNE  = nullptr;
    SDL_Texture* texNoteRed_LNS   = nullptr;
    SDL_Texture* texNoteRed_LNE   = nullptr;

    SDL_Texture* texKeybeamWhite = nullptr;
    SDL_Texture* texKeybeamBlue  = nullptr;
    SDL_Texture* texKeybeamRed   = nullptr;

    SDL_Texture* texJudgeAtlas = nullptr;
    SDL_Texture* texNumberAtlas = nullptr;

    std::vector<SDL_Texture*> texBombs; 
    SDL_Texture* texLaneCover = nullptr; 
    SDL_Texture* texGaugeAssist = nullptr;
    SDL_Texture* texGaugeNormal = nullptr;
    SDL_Texture* texGaugeHard   = nullptr;
    SDL_Texture* texGaugeExHard = nullptr;
    SDL_Texture* texGaugeHazard = nullptr;
    SDL_Texture* texGaugeDan    = nullptr;
    SDL_Texture* texKeys = nullptr;
    SDL_Texture* tex_scratch = nullptr;
    
};

#endif