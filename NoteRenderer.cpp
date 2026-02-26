#include "NoteRenderer.hpp"
#include "Config.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <SDL2/SDL_image.h>

/**
 * 左右対称レイアウトのためのX座標計算
 */
static int getBaseX() {
    int sw = Config::SCRATCH_WIDTH;
    int lw = Config::LANE_WIDTH;
    int totalWidth = (7 * lw) + sw + 10;
    int rightPadding = 50; 
    
    return (Config::PLAY_SIDE == 1) ? 50 : (Config::SCREEN_WIDTH - totalWidth - rightPadding);
}

/**
 * BGAやUIを表示すべき中心X座標を計算する
 */
static int getBGACenterX() {
    int sw = Config::SCRATCH_WIDTH;
    int lw = Config::LANE_WIDTH;
    int totalWidth = (7 * lw) + sw + 10;
    int startX = getBaseX();

    if (Config::PLAY_SIDE == 1) {
        int laneRightEdge = startX + totalWidth;
        return laneRightEdge + (Config::SCREEN_WIDTH - laneRightEdge) / 2;
    } else {
        return startX / 2;
    }
}

static int getXForLane(int lane) {
    int startX = getBaseX();
    int sw = Config::SCRATCH_WIDTH;
    int lw = Config::LANE_WIDTH;
    int pad = 0; 

    if (Config::PLAY_SIDE == 1) { // 1P SIDE
        if (lane == 8) return startX;
        return startX + sw + pad + (lane - 1) * lw;
    } else { // 2P SIDE
        if (lane == 8) return startX + (7 * lw) + pad;
        return startX + (lane - 1) * lw;
    }
}

void NoteRenderer::init(SDL_Renderer* ren) {
    TTF_Init();
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    fontSmall = TTF_OpenFont(Config::FONT_PATH.c_str(), 24);
    fontBig = TTF_OpenFont(Config::FONT_PATH.c_str(), 48);

    std::string s = Config::ROOT_PATH + "Skin/";
    texNoteWhite = IMG_LoadTexture(ren, (s + "note_white.png").c_str());
    texNoteBlue  = IMG_LoadTexture(ren, (s + "note_blue.png").c_str());
    texNoteRed   = IMG_LoadTexture(ren, (s + "note_red.png").c_str());
    
    texNoteWhite_LN = IMG_LoadTexture(ren, (s + "note_white_ln.png").c_str());
    texNoteWhite_LN_Active1 = IMG_LoadTexture(ren, (s + "note_white_ln_active1.png").c_str());
    texNoteWhite_LN_Active2 = IMG_LoadTexture(ren, (s + "note_white_ln_active2.png").c_str());
    texNoteBlue_LN  = IMG_LoadTexture(ren, (s + "note_blue_ln.png").c_str());
    texNoteBlue_LN_Active1  = IMG_LoadTexture(ren, (s + "note_blue_ln_active1.png").c_str());
    texNoteBlue_LN_Active2  = IMG_LoadTexture(ren, (s + "note_blue_ln_active2.png").c_str());
    texNoteRed_LN   = IMG_LoadTexture(ren, (s + "note_red_ln.png").c_str());
    texNoteRed_LN_Active1   = IMG_LoadTexture(ren, (s + "note_red_ln_active1.png").c_str());
    texNoteRed_LN_Active2   = IMG_LoadTexture(ren, (s + "note_red_ln_active2.png").c_str());

    texNoteWhite_LNS = IMG_LoadTexture(ren, (s + "note_white_lns.png").c_str());
    texNoteWhite_LNE = IMG_LoadTexture(ren, (s + "note_white_lne.png").c_str());
    texNoteBlue_LNS  = IMG_LoadTexture(ren, (s + "note_blue_lns.png").c_str());
    texNoteBlue_LNE  = IMG_LoadTexture(ren, (s + "note_blue_lne.png").c_str());
    texNoteRed_LNS   = IMG_LoadTexture(ren, (s + "note_red_lns.png").c_str());
    texNoteRed_LNE   = IMG_LoadTexture(ren, (s + "note_red_lne.png").c_str());

    texKeybeamWhite = IMG_LoadTexture(ren, (s + "beam_white.png").c_str());
    texKeybeamBlue  = IMG_LoadTexture(ren, (s + "beam_blue.png").c_str());
    texKeybeamRed   = IMG_LoadTexture(ren, (s + "beam_red.png").c_str());

    texJudgeAtlas = IMG_LoadTexture(ren, (s + "judge.png").c_str());
    texNumberAtlas = IMG_LoadTexture(ren, (s + "judge_number.png").c_str());

    // --- 追加: レーンカバー画像のロード ---
    texLaneCover = IMG_LoadTexture(ren, (s + "lanecover.png").c_str());

    texBombs.clear();
    for (int i = 0; i < 10; i++) {
        std::string bombPath = s + "bomb_" + std::to_string(i) + ".png";
        SDL_Texture* t = IMG_LoadTexture(ren, bombPath.c_str());
        if (t) {
            texBombs.push_back(t);
        } else {
            break; 
        }
    }
}

void NoteRenderer::cleanup() {
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (fontBig) TTF_CloseFont(fontBig);

    if (texNoteWhite) SDL_DestroyTexture(texNoteWhite);
    if (texNoteBlue)  SDL_DestroyTexture(texNoteBlue);
    if (texNoteRed)   SDL_DestroyTexture(texNoteRed);
    
    if (texNoteWhite_LN) SDL_DestroyTexture(texNoteWhite_LN);
    if (texNoteWhite_LN_Active1) SDL_DestroyTexture(texNoteWhite_LN_Active1);
    if (texNoteWhite_LN_Active2) SDL_DestroyTexture(texNoteWhite_LN_Active2);
    if (texNoteBlue_LN)  SDL_DestroyTexture(texNoteBlue_LN);
    if (texNoteBlue_LN_Active1)  SDL_DestroyTexture(texNoteBlue_LN_Active1);
    if (texNoteBlue_LN_Active2)  SDL_DestroyTexture(texNoteBlue_LN_Active2);
    if (texNoteRed_LN)   SDL_DestroyTexture(texNoteRed_LN);
    if (texNoteRed_LN_Active1)   SDL_DestroyTexture(texNoteRed_LN_Active1);
    if (texNoteRed_LN_Active2)   SDL_DestroyTexture(texNoteRed_LN_Active2);

    if (texNoteWhite_LNS) SDL_DestroyTexture(texNoteWhite_LNS);
    if (texNoteWhite_LNE) SDL_DestroyTexture(texNoteWhite_LNE);
    if (texNoteBlue_LNS)  SDL_DestroyTexture(texNoteBlue_LNS);
    if (texNoteBlue_LNE)  SDL_DestroyTexture(texNoteBlue_LNE);
    if (texNoteRed_LNS)   SDL_DestroyTexture(texNoteRed_LNS);
    if (texNoteRed_LNE)   SDL_DestroyTexture(texNoteRed_LNE);

    if (texKeybeamWhite) SDL_DestroyTexture(texKeybeamWhite);
    if (texKeybeamBlue)  SDL_DestroyTexture(texKeybeamBlue);
    if (texKeybeamRed)   SDL_DestroyTexture(texKeybeamRed);

    if (texJudgeAtlas) SDL_DestroyTexture(texJudgeAtlas);
    if (texNumberAtlas) SDL_DestroyTexture(texNumberAtlas);

    // --- 追加: レーンカバー画像の破棄 ---
    if (texLaneCover) SDL_DestroyTexture(texLaneCover);

    for (auto t : texBombs) {
        if (t) SDL_DestroyTexture(t);
    }
    texBombs.clear();

    for (auto& pair : textureCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    std::map<std::string, SDL_Texture*>().swap(textureCache);

    for (auto& pair : textTextureCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    std::map<std::string, SDL_Texture*>().swap(textTextureCache);

    for (auto& pair : customFontCache) {
        if (pair.second) TTF_CloseFont(pair.second);
    }
    std::map<std::string, TTF_Font*>().swap(customFontCache);

    IMG_Quit();
    TTF_Quit();
}

void NoteRenderer::drawText(SDL_Renderer* ren, const std::string& text, int x, int y, SDL_Color color, bool isBig, bool isCenter, bool isRight, const std::string& fontPath) {
    if (text.empty()) return;
    TTF_Font* targetFont = isBig ? fontBig : fontSmall;
    if (!fontPath.empty()) {
        if (customFontCache.find(fontPath) == customFontCache.end()) {
            TTF_Font* cf = TTF_OpenFont(fontPath.c_str(), isBig ? 48 : 24);
            if (cf) customFontCache[fontPath] = cf;
        }
        if (customFontCache.count(fontPath)) targetFont = customFontCache[fontPath];
    }
    if (!targetFont) return;
    SDL_Surface* s = TTF_RenderUTF8_Blended(targetFont, text.c_str(), color);
    if (!s) return;
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    if (t) {
        int drawX = x;
        if (isCenter) drawX = x - s->w / 2;
        else if (isRight) drawX = x - s->w;
        SDL_Rect dst = { drawX, y, s->w, s->h };
        SDL_RenderCopy(ren, t, NULL, &dst);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

void NoteRenderer::drawTextCached(SDL_Renderer* ren, const std::string& text, int x, int y, SDL_Color color, bool isBig, bool isCenter, bool isRight, const std::string& fontPath) {
    if (text.empty()) return;
    std::string key = text + "_" + std::to_string(color.r) + std::to_string(color.g) + 
                      std::to_string(color.b) + std::to_string(color.a) + 
                      (isBig ? "_B" : "_S") + fontPath;
    SDL_Texture* t = nullptr;
    int texW = 0, texH = 0;
    if (textTextureCache.count(key)) {
        t = textTextureCache[key];
        SDL_QueryTexture(t, NULL, NULL, &texW, &texH);
    } else {
        TTF_Font* targetFont = isBig ? fontBig : fontSmall;
        if (!fontPath.empty()) {
            if (customFontCache.find(fontPath) == customFontCache.end()) {
                TTF_Font* cf = TTF_OpenFont(fontPath.c_str(), isBig ? 48 : 24);
                if (cf) customFontCache[fontPath] = cf;
            }
            if (customFontCache.count(fontPath)) targetFont = customFontCache[fontPath];
        }
        if (!targetFont) return;
        SDL_Surface* s = TTF_RenderUTF8_Blended(targetFont, text.c_str(), color);
        if (!s) return;
        t = SDL_CreateTextureFromSurface(ren, s);
        texW = s->w; texH = s->h;
        SDL_FreeSurface(s);
        if (t) textTextureCache[key] = t;
    }
    if (t) {
        int drawX = x;
        if (isCenter) drawX = x - texW / 2;
        else if (isRight) drawX = x - texW;
        SDL_Rect dst = { drawX, y, texW, texH };
        SDL_RenderCopy(ren, t, NULL, &dst);
    }
}

void NoteRenderer::drawImage(SDL_Renderer* ren, const std::string& path, int x, int y, int w, int h, int alpha) {
    if (path.empty()) return;
    if (textureCache.find(path) == textureCache.end()) {
        SDL_Texture* t = IMG_LoadTexture(ren, path.c_str());
        if (t) textureCache[path] = t;
    }
    if (textureCache.count(path)) {
        SDL_Texture* t = textureCache[path];
        SDL_SetTextureAlphaMod(t, (Uint8)alpha);
        SDL_Rect dst = { x, y, w, h };
        SDL_RenderCopy(ren, t, NULL, &dst);
    }
}

void NoteRenderer::renderDecisionInfo(SDL_Renderer* ren, const BMSHeader& header) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {180, 180, 180, 255};
    SDL_Color yellow = {255, 255, 0, 255};
    int centerX = 640;
    drawTextCached(ren, header.genre, centerX, 220, gray, false, true);
    drawTextCached(ren, header.title, centerX, 270, white, true, true);
    drawTextCached(ren, header.artist, centerX, 340, white, false, true);
    std::string levelInfo = "[" + header.chartName + "]  LEVEL " + std::to_string(header.level);
    drawTextCached(ren, levelInfo, centerX, 375, yellow, false, true);
    char bpmBuf[64];
    if (std::abs(header.min_bpm - header.max_bpm) < 0.1) {
        snprintf(bpmBuf, sizeof(bpmBuf), "BPM %.0f", header.min_bpm);
    } else {
        snprintf(bpmBuf, sizeof(bpmBuf), "BPM %.0f - %.0f", header.min_bpm, header.max_bpm);
    }
    drawText(ren, bpmBuf, centerX, 410, white, false, true);
}

void NoteRenderer::renderLanes(SDL_Renderer* ren, double progress) {
    int sw = Config::SCRATCH_WIDTH;
    int lw = Config::LANE_WIDTH;
    int totalWidth = (7 * lw) + sw + 10;
    int startX = getBaseX(); 
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    SDL_Rect overallBg = { startX, 0, totalWidth, Config::SCREEN_HEIGHT };
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, &overallBg);
    int progX = (Config::PLAY_SIDE == 1) ? startX - 12 : startX + totalWidth + 4;
    int progY = 100, progH = 500, indicatorH = 8;
    SDL_Rect progFrame = { progX, progY, 6, progH };
    SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
    SDL_RenderDrawRect(ren, &progFrame);
    int moveY = progY + (int)((progH - indicatorH) * progress);
    SDL_Rect progIndicator = { progX + 1, moveY, 4, indicatorH };
    SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
    SDL_RenderFillRect(ren, &progIndicator);
    SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
    for (int i = 1; i <= 8; i++) {
        int lx = getXForLane(i);
        SDL_RenderDrawLine(ren, lx, 0, lx, Config::SCREEN_HEIGHT);
    }
    int rightEdge = (Config::PLAY_SIDE == 1) ? getXForLane(7) + lw : getXForLane(8) + sw;
    SDL_RenderDrawLine(ren, rightEdge, 0, rightEdge, Config::SCREEN_HEIGHT);
    SDL_SetRenderDrawColor(ren, 255, 0, 0, 255);
    SDL_RenderDrawLine(ren, startX, judgeY, startX + totalWidth, judgeY);
    
    // --- SUDDEN+ (レーンカバー) 描画部分の修正 ---
    if (Config::SUDDEN_PLUS > 0) {
        SDL_Rect suddenRect = { startX, 0, totalWidth, Config::SUDDEN_PLUS };
        if (texLaneCover) {
            // 画像を引き伸ばして描画
            SDL_RenderCopy(ren, texLaneCover, NULL, &suddenRect);
        } else {
            // フォールバック
            SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
            SDL_RenderFillRect(ren, &suddenRect);
        }
    }
    
    if (Config::LIFT > 0) {
        SDL_Rect liftRect = { startX, judgeY, totalWidth, Config::SCREEN_HEIGHT - judgeY };
        SDL_SetRenderDrawColor(ren, 20, 20, 20, 255);
        SDL_RenderFillRect(ren, &liftRect);
    }
}

void NoteRenderer::renderNote(SDL_Renderer* ren, const PlayableNote& note, double cur_ms, double speed, bool isAuto) {
    int x = getXForLane(note.lane);
    int w = (note.lane == 8) ? Config::SCRATCH_WIDTH : Config::LANE_WIDTH;
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    double diff = note.target_ms - cur_ms;
    int baseY = judgeY - (int)(diff * speed);
    int y = baseY - 8 - (int)Config::JUDGE_OFFSET;
    int headY = (note.isLN && note.isBeingPressed) ? (judgeY - 8 - (int)Config::JUDGE_OFFSET) : y;
    SDL_Texture* targetTex = nullptr; SDL_Texture* lnBodyNormal = nullptr; SDL_Texture* lnBodyActive1 = nullptr; SDL_Texture* lnBodyActive2 = nullptr; SDL_Texture* startTex = nullptr; SDL_Texture* endTex = nullptr;
    if (note.lane == 8) { targetTex = texNoteRed; lnBodyNormal = texNoteRed_LN; lnBodyActive1 = texNoteRed_LN_Active1; lnBodyActive2 = texNoteRed_LN_Active2; startTex = texNoteRed_LNS; endTex = texNoteRed_LNE; }
    else if (note.lane % 2 == 0) { targetTex = texNoteBlue; lnBodyNormal = texNoteBlue_LN; lnBodyActive1 = texNoteBlue_LN_Active1; lnBodyActive2 = texNoteBlue_LN_Active2; startTex = texNoteBlue_LNS; endTex = texNoteBlue_LNE; }
    else { targetTex = texNoteWhite; lnBodyNormal = texNoteWhite_LN; lnBodyActive1 = texNoteWhite_LN_Active1; lnBodyActive2 = texNoteWhite_LN_Active2; startTex = texNoteWhite_LNS; endTex = texNoteWhite_LNE; }
    
    if (note.isLN) {
        double endDiff = (note.target_ms + note.duration_ms) - cur_ms;
        int tailY = judgeY - (int)(endDiff * speed) - 8 - (int)Config::JUDGE_OFFSET;
        if (!(tailY > judgeY || headY < Config::SUDDEN_PLUS - 20)) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_Texture* currentBodyTex = lnBodyNormal;
            if (note.isBeingPressed) { currentBodyTex = ((SDL_GetTicks() / 100) % 2 == 0) ? lnBodyActive1 : lnBodyActive2; }
            int drawTailY = std::max(tailY + 4, Config::SUDDEN_PLUS);
            int drawHeadY = std::min(headY + 4, judgeY);
            if (drawHeadY > drawTailY) {
                SDL_Rect bodyRect = { x + 4, drawTailY, w - 8, drawHeadY - drawTailY };
                if (currentBodyTex) { SDL_SetTextureAlphaMod(currentBodyTex, 255); SDL_RenderCopy(ren, currentBodyTex, NULL, &bodyRect); }
                else {
                    if (isAuto) SDL_SetRenderDrawColor(ren, 0, 200, 0, 255);
                    else if (note.isBeingPressed) SDL_SetRenderDrawColor(ren, 255, 255, 100, 220);
                    else if (note.lane == 8) SDL_SetRenderDrawColor(ren, 180, 30, 30, 150);
                    else if (note.lane % 2 == 0) SDL_SetRenderDrawColor(ren, 40, 70, 180, 150);
                    else SDL_SetRenderDrawColor(ren, 180, 180, 180, 150);
                    SDL_RenderFillRect(ren, &bodyRect);
                }
            }
            if (tailY >= Config::SUDDEN_PLUS && tailY <= judgeY) {
                SDL_Rect tailRect = { x + 2, tailY, w - 4, 8 };
                SDL_Texture* finalEndTex = endTex ? endTex : targetTex;
                if (finalEndTex) SDL_RenderCopy(ren, finalEndTex, NULL, &tailRect);
            }
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }
    }
    if (headY < Config::SUDDEN_PLUS || headY > judgeY) return;
    SDL_Rect rect = { x + 2, headY, w - 4, 8 };
    SDL_Texture* finalHeadTex = (note.isLN && startTex) ? startTex : targetTex;
    if (finalHeadTex) { if (isAuto) SDL_SetTextureAlphaMod(finalHeadTex, 160); SDL_RenderCopy(ren, finalHeadTex, NULL, &rect); if (isAuto) SDL_SetTextureAlphaMod(finalHeadTex, 255); }
}

void NoteRenderer::renderBeatLine(SDL_Renderer* ren, double diff, double speed) {
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int y = judgeY - (int)(diff * speed) - (int)Config::JUDGE_OFFSET;
    if (y < Config::SUDDEN_PLUS || y > judgeY) return;
    int totalWidth = (7 * Config::LANE_WIDTH) + Config::SCRATCH_WIDTH + 10;
    int startX = getBaseX();
    SDL_SetRenderDrawColor(ren, 60, 60, 70, 255);
    SDL_RenderDrawLine(ren, startX, y, startX + totalWidth, y);
}

void NoteRenderer::renderHitEffect(SDL_Renderer* ren, int lane, float progress) {
    int fullW = (lane == 8) ? Config::SCRATCH_WIDTH : Config::LANE_WIDTH;
    int centerX = getXForLane(lane) + fullW / 2;
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int currentW = (int)(fullW * (1.0f - progress));
    if (currentW < 1) return;
    int alpha = (int)((1.0f - progress) * 200);
    int beamDisplayH = 300; 
    SDL_Texture* targetBeam = (lane == 8) ? texKeybeamRed : (lane % 2 == 0 ? texKeybeamBlue : texKeybeamWhite);
    if (targetBeam) {
        SDL_SetTextureBlendMode(targetBeam, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(targetBeam, (Uint8)alpha);
        SDL_Rect rect = { centerX - currentW / 2, judgeY - beamDisplayH, currentW, beamDisplayH };
        SDL_RenderCopy(ren, targetBeam, NULL, &rect);
    }
}

void NoteRenderer::renderBomb(SDL_Renderer* ren, int lane, int frame) {
    int fullW = (lane == 8) ? Config::SCRATCH_WIDTH : Config::LANE_WIDTH;
    int x = getXForLane(lane);
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int size = (int)(Config::LANE_WIDTH * 3.0f); 
    if (!texBombs.empty() && frame >= 0 && frame < (int)texBombs.size()) {
        SDL_Texture* t = texBombs[frame];
        SDL_Rect rect = { x + (fullW / 2) - (size / 2), judgeY - (size / 2), size, size };
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
        SDL_RenderCopy(ren, t, NULL, &rect);
    }
}

void NoteRenderer::renderJudgment(SDL_Renderer* ren, const std::string& text, float progress, SDL_Color color, int combo) {
    if (text.empty() || !texJudgeAtlas || !texNumberAtlas) return;
    int judgeType = 4;
    if (text == "P-GREAT") judgeType = 3;
    else if (text == "GREAT") judgeType = 2;
    else if (text == "GOOD") judgeType = 1;
    else if (text == "BAD") judgeType = 0;
    int atlasW, atlasH;
    SDL_QueryTexture(texJudgeAtlas, NULL, NULL, &atlasW, &atlasH);
    int jw = atlasW / 7; int jh = atlasH;
    int numAtlasW, numAtlasH;
    SDL_QueryTexture(texNumberAtlas, NULL, NULL, &numAtlasW, &numAtlasH);
    int nw = numAtlasW / 4; int nh = numAtlasH / 10; 
    int judgeIdx = 0; int colorCol = 3; 
    if (judgeType == 3) { int animFrame = (SDL_GetTicks() / 50) % 3; judgeIdx = animFrame; colorCol = animFrame; } 
    else if (judgeType == 2) judgeIdx = 3; else if (judgeType == 1) judgeIdx = 4; else if (judgeType == 0) judgeIdx = 5; else judgeIdx = 6; 
    float scale = 0.6f;
    int drawJW = (int)(jw * scale); int drawJH = (int)(jh * scale);
    int drawNW = (int)(nw * scale); int drawNH = (int)(nh * scale);
    const int numKerning = 10;
    std::string comboStr = (combo > 0) ? std::to_string(combo) : "";
    int spacing = 0; int totalWidth = drawJW;
    if (!comboStr.empty()) {
        int comboWidth = (int)comboStr.length() * drawNW - ((int)comboStr.length() - 1) * numKerning;
        totalWidth += spacing + comboWidth;
    }
    int laneAreaW = (7 * Config::LANE_WIDTH) + Config::SCRATCH_WIDTH + 10;
    int centerX = getBaseX() + laneAreaW / 2;
    int startX = centerX - totalWidth / 2;
    int drawY = 400 - Config::LIFT;
    Uint8 alpha = (Uint8)(255 * (1.0f - progress));
    SDL_Rect jSrc = { judgeIdx * jw, 0, jw, jh };
    SDL_Rect jDst = { startX, drawY, drawJW, drawJH };
    SDL_SetTextureAlphaMod(texJudgeAtlas, alpha);
    SDL_RenderCopy(ren, texJudgeAtlas, &jSrc, &jDst);
    if (!comboStr.empty()) {
        int curX = startX + drawJW + spacing;
        SDL_SetTextureAlphaMod(texNumberAtlas, alpha);
        for (char c : comboStr) {
            int n = c - '0';
            SDL_Rect nSrc = { colorCol * nw, n * nh, nw, nh };
            SDL_Rect nDst = { curX, drawY - (drawNH - drawJH) / 2, drawNW, drawNH };
            SDL_RenderCopy(ren, texNumberAtlas, &nSrc, &nDst);
            curX += (drawNW - numKerning);
        }
    }
}

void NoteRenderer::renderCombo(SDL_Renderer* ren, int combo) {}

void NoteRenderer::renderGauge(SDL_Renderer* ren, double gaugeValue, int gaugeOption, bool isFailed) {
    int sw = Config::SCRATCH_WIDTH; int lw = Config::LANE_WIDTH; int totalWidth = (7 * lw) + sw + 10;
    int gaugeX = getBaseX(); int gaugeY = Config::JUDGMENT_LINE_Y + 45; int gaugeH = 12;
    int segCount = 50; int segW = totalWidth / segCount;
    SDL_Rect bg = { gaugeX, gaugeY, totalWidth, gaugeH };
    SDL_SetRenderDrawColor(ren, 20, 20, 20, 255); SDL_RenderFillRect(ren, &bg);
    for (int i = 0; i < segCount; i++) {
        double threshold = (i + 1) * 2.0;
        if (gaugeValue >= threshold) {
            if (isFailed) SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
            else {
                switch (gaugeOption) {
                    case 1: if (i < 30) SDL_SetRenderDrawColor(ren, 0, 180, 255, 255); else SDL_SetRenderDrawColor(ren, 220, 0, 80, 255); break;
                    case 3: case 5: SDL_SetRenderDrawColor(ren, 220, 0, 0, 255); break;
                    case 4: SDL_SetRenderDrawColor(ren, 255, 215, 0, 255); break;
                    default: if (i < 40) SDL_SetRenderDrawColor(ren, 0, 180, 255, 255); else SDL_SetRenderDrawColor(ren, 220, 0, 80, 255); break;
                }
            }
            int drawX = (Config::PLAY_SIDE == 1) ? gaugeX + (i * segW) : (gaugeX + totalWidth) - ((i + 1) * segW);
            SDL_Rect seg = { drawX + 1, gaugeY + 1, segW - 1, gaugeH - 2 }; SDL_RenderFillRect(ren, &seg);
        }
    }
    char gStr[16];
    if (Config::GAUGE_DISPLAY_TYPE == 1) { int displayVal = (gaugeValue >= 100.0) ? 100 : ((int)gaugeValue / 2) * 2; snprintf(gStr, sizeof(gStr), "%3d%%", displayVal); }
    else { snprintf(gStr, sizeof(gStr), "%5.1f%%", gaugeValue); }
    drawText(ren, gStr, (Config::PLAY_SIDE == 1) ? (gaugeX + totalWidth - 65) : gaugeX, gaugeY + 15, {255, 255, 255, 255}, false, false);
}

void NoteRenderer::renderUI(SDL_Renderer* ren, const BMSHeader& header, int fps, double bpm, int exScore) {
    int centerX = getBGACenterX();
    drawTextCached(ren, header.genre, centerX, 10, {180, 180, 180, 255}, false, true);
    std::string titleFull = header.title + " [" + header.chartName + " LV:" + std::to_string(header.level) + "]";
    drawTextCached(ren, titleFull, centerX, 35, {255, 255, 255, 255}, false, true);
    drawTextCached(ren, header.artist, centerX, 60, {255, 255, 255, 255}, false, true);
    char info[128]; snprintf(info, sizeof(info), "SCORE %06d | BPM %3.0f | FPS %d", exScore, bpm, fps);
    drawText(ren, info, centerX, Config::SCREEN_HEIGHT - 35, {255, 255, 255, 255}, false, true);
}

void NoteRenderer::renderLoading(SDL_Renderer* ren, int current, int total, const std::string& filename) {
    SDL_SetRenderDrawColor(ren, 0, 0, 5, 255); SDL_RenderClear(ren);
    drawText(ren, "NOW LOADING...", 640, 300, {255, 255, 255, 255}, true, true);
    SDL_Rect barOut = { 100, 380, 1080, 20 }; SDL_SetRenderDrawColor(ren, 40, 40, 40, 255); SDL_RenderDrawRect(ren, &barOut);
    float prog = (total > 0) ? (float)current / total : 0;
    SDL_Rect barIn = { 102, 382, (int)(1076 * prog), 16 }; SDL_SetRenderDrawColor(ren, 0, 120, 255, 255); SDL_RenderFillRect(ren, &barIn);
    drawText(ren, filename, 640, 420, {150, 150, 150, 255}, false, true);
}

void NoteRenderer::renderResult(SDL_Renderer* ren, const PlayStatus& status, const BMSHeader& header, const std::string& rank) {
    SDL_SetRenderDrawColor(ren, 5, 5, 10, 255); SDL_RenderClear(ren);
    SDL_Color white = {255, 255, 255, 255}, yellow = {255, 255, 0, 255};
    drawTextCached(ren, header.title, 640, 50, white, true, true);
    drawTextCached(ren, "RANK: " + rank, 640, 120, yellow, true, true);
    int startY = 240, spacing = 45;
    drawText(ren, "P-GREAT : " + std::to_string(status.pGreatCount), 400, startY, white, false, false);
    drawText(ren, "GREAT    : " + std::to_string(status.greatCount), 400, startY + spacing, white, false, false);
    drawText(ren, "GOOD     : " + std::to_string(status.goodCount), 400, startY + spacing * 2, white, false, false);
    drawText(ren, "BAD      : " + std::to_string(status.badCount), 400, startY + spacing * 3, white, false, false);
    drawText(ren, "POOR     : " + std::to_string(status.poorCount), 400, startY + spacing * 4, white, false, false);
    int exScore = (status.pGreatCount * 2) + status.greatCount;
    drawText(ren, "MAX COMBO : " + std::to_string(status.maxCombo), 680, startY, yellow, false, false);
    drawText(ren, "EX SCORE  : " + std::to_string(exScore), 680, startY + spacing, white, false, false);
    std::string clearText = "FAILED"; SDL_Color clearColor = {100, 100, 100, 255};
    switch (status.clearType) {
        case ClearType::FULL_COMBO: clearText = "FULL COMBO"; clearColor = {255, 255, 255, 255}; break;
        case ClearType::EX_HARD_CLEAR: clearText = "EX-HARD CLEAR"; clearColor = {255, 255, 0, 255}; break;
        case ClearType::HARD_CLEAR: clearText = "HARD CLEAR"; clearColor = {255, 0, 0, 255}; break;
        case ClearType::NORMAL_CLEAR: clearText = "NORMAL CLEAR"; clearColor = {0, 200, 255, 255}; break;
        case ClearType::EASY_CLEAR: clearText = "EASY CLEAR"; clearColor = {150, 255, 100, 255}; break;
        case ClearType::ASSIST_CLEAR: clearText = "ASSIST CLEAR"; clearColor = {180, 100, 255, 255}; break;
        case ClearType::DAN_CLEAR: clearText = "DAN CLEAR"; clearColor = {200, 0, 100, 255}; break;
        default: break;
    }
    drawTextCached(ren, clearText, 640, 550, clearColor, true, true);
    if ((SDL_GetTicks() / 500) % 2 == 0) drawTextCached(ren, "PRESS ANY BUTTON TO EXIT", 640, 650, {150, 150, 150, 255}, true, true);
}