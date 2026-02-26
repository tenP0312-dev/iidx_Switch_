#include "NoteRenderer.hpp"
#include "Config.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <SDL2/SDL_image.h>

static int getWidthForLane(int lane) {
    if (lane == 8) return Config::SCRATCH_WIDTH;
    return (lane % 2 != 0) ? (int)(Config::LANE_WIDTH * 1.4) : Config::LANE_WIDTH;
}

static int getBaseX() {
    int sw = Config::SCRATCH_WIDTH;
    int totalKeysWidth = 0;
    for (int i = 1; i <= 7; i++) totalKeysWidth += getWidthForLane(i);
    int totalWidth = totalKeysWidth + sw;
    return (Config::PLAY_SIDE == 1) ? 50 : (Config::SCREEN_WIDTH - totalWidth - 50);
}

static int getBGACenterX() {
    int sw = Config::SCRATCH_WIDTH;
    int totalKeysWidth = 0;
    for (int i = 1; i <= 7; i++) totalKeysWidth += getWidthForLane(i);
    int totalWidth = totalKeysWidth + sw;
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
    if (Config::PLAY_SIDE == 1) {
        if (lane == 8) return startX;
        int x = startX + sw;
        for (int i = 1; i < lane; i++) x += getWidthForLane(i);
        return x;
    } else {
        if (lane == 8) {
            int totalKeysWidth = 0;
            for (int i = 1; i <= 7; i++) totalKeysWidth += getWidthForLane(i);
            return startX + totalKeysWidth;
        }
        int x = startX;
        for (int i = 1; i < lane; i++) x += getWidthForLane(i);
        return x;
    }
}

//画像類の読み込み
void NoteRenderer::init(SDL_Renderer* ren) {
    TTF_Init();
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    fontSmall = TTF_OpenFont(Config::FONT_PATH.c_str(), 24);
    fontBig = TTF_OpenFont(Config::FONT_PATH.c_str(), 48);

    std::string s = Config::ROOT_PATH + "Skin/";
    texBackground = IMG_LoadTexture(ren, (s + "Flame_BG.png").c_str());
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
    texLaneCover = IMG_LoadTexture(ren, (s + "lanecover.png").c_str());

    texGaugeAssist = IMG_LoadTexture(ren, (s + "gauge_assist.png").c_str());
    texGaugeNormal = IMG_LoadTexture(ren, (s + "gauge_normal.png").c_str());
    texGaugeHard   = IMG_LoadTexture(ren, (s + "gauge_hard.png").c_str());
    texGaugeExHard = IMG_LoadTexture(ren, (s + "gauge_exhard.png").c_str());
    texGaugeHazard = IMG_LoadTexture(ren, (s + "gauge_hazard.png").c_str());
    texGaugeDan    = IMG_LoadTexture(ren, (s + "gauge_dan.png").c_str());

    texKeys = IMG_LoadTexture(ren, (s + "7keypad.png").c_str());

    lane_Flame = IMG_LoadTexture(ren, (s + "lane_Flame.png").c_str());
    lane_Flame2 = IMG_LoadTexture(ren, (s + "lane_Flame2.png").c_str());

    for (int i = 0; i < 10; i++) {
    std::string bombPath = s + "bomb_" + std::to_string(i) + ".png";
    SDL_Texture* t = IMG_LoadTexture(ren, bombPath.c_str());
    if (t) texBombs.push_back(t);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    tex_scratch = IMG_LoadTexture(ren, (s + "scratch.png").c_str());
    if (tex_scratch) {
        // SDL 2.0.12以降で利用可能なScaleModeBest（ミップマップや最高品質補間）を適用
        SDL_SetTextureScaleMode(tex_scratch, SDL_ScaleModeBest);
        // ブレンドモードも確実に設定し、透過エッジの品質を担保
        SDL_SetTextureBlendMode(tex_scratch, SDL_BLENDMODE_BLEND);
    }
    // 後続の画像のために品質設定をデフォルト(Linear)に戻す
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
}

void NoteRenderer::clearTextCache() {
    for (auto& pair : textTextureCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    textTextureCache.clear();
}

void NoteRenderer::cleanup() {
    clearTextCache();
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (fontBig) TTF_CloseFont(fontBig);
    if (texBackground) SDL_DestroyTexture(texBackground);
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
    if (texLaneCover) SDL_DestroyTexture(texLaneCover);
    if (texGaugeAssist) SDL_DestroyTexture(texGaugeAssist);
    if (texGaugeNormal) SDL_DestroyTexture(texGaugeNormal);
    if (texGaugeHard)   SDL_DestroyTexture(texGaugeHard);
    if (texGaugeExHard) SDL_DestroyTexture(texGaugeExHard);
    if (texGaugeHazard) SDL_DestroyTexture(texGaugeHazard);
    if (texGaugeDan)    SDL_DestroyTexture(texGaugeDan);
    if (texKeys) SDL_DestroyTexture(texKeys);
    if (lane_Flame) {SDL_DestroyTexture(lane_Flame);lane_Flame = nullptr;}
    if (lane_Flame2) {SDL_DestroyTexture(lane_Flame2);lane_Flame2 = nullptr;}
    if (tex_scratch) SDL_DestroyTexture(tex_scratch);

    for (auto t : texBombs) if (t) SDL_DestroyTexture(t);
    texBombs.clear();
    for (auto& pair : textureCache) if (pair.second) SDL_DestroyTexture(pair.second);
    textureCache.clear();
    for (auto& pair : customFontCache) if (pair.second) TTF_CloseFont(pair.second);
    customFontCache.clear();
    IMG_Quit();
    TTF_Quit();
}

void NoteRenderer::renderBackground(SDL_Renderer* ren) {
    if (texBackground) {
        if (Config::PLAY_SIDE == 1) {
            // 1P: 通常描画
            SDL_RenderCopy(ren, texBackground, NULL, NULL);
        } else {
            // 2P: 左右反転して描画
            SDL_RenderCopyEx(ren, texBackground, NULL, NULL, 0, NULL, SDL_FLIP_HORIZONTAL);
        }
    }
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

        if (textTextureCache.size() > 128){
            clearTextCache();
        }

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

void NoteRenderer::renderUI(SDL_Renderer* ren, const BMSHeader& header, int fps, double bpm, int exScore) {
    int centerX = getBGACenterX();
    
    // ネームプレートのパス
    std::string platePath = Config::ROOT_PATH + "Skin/Flame_nameplate.png";
    
    // 1. 画像をキャッシュに読み込み/取得
    if (textureCache.find(platePath) == textureCache.end()) {
        SDL_Texture* t = IMG_LoadTexture(ren, platePath.c_str());
        if (t) textureCache[platePath] = t;
    }

    // 2. テクスチャが存在すればサイズを取得して描画
    if (textureCache.count(platePath)) {
        SDL_Texture* t = textureCache[platePath];
        int origW, origH;
        SDL_QueryTexture(t, NULL, NULL, &origW, &origH);

        // Y=0固定で中央に配置
        int plateX = centerX - origW / 2;
        int plateY = 0; 
        
        SDL_Rect dst = { plateX, plateY, origW, origH };
        SDL_RenderCopy(ren, t, NULL, &dst);
    }

    // 3. テキスト描画（位置は既存ロジックを維持）
    //drawTextCached(ren, header.genre, centerX, 10, {180, 180, 180, 255}, false, true);
    //drawTextCached(ren, header.title + " [" + header.chartName + " LV:" + std::to_string(header.level) + "]", centerX, 35, {255, 255, 255, 255}, false, true);
    //drawTextCached(ren, header.artist, centerX, 60, {255, 255, 255, 255}, false, true);

    //char info[128]; 
    //snprintf(info, sizeof(info), "SCORE %06d | BPM %.0f | FPS %d", exScore, bpm, fps);
    //drawTextCached(ren, info, centerX, Config::SCREEN_HEIGHT - 35, {255, 255, 255, 255}, false, true);
}

void NoteRenderer::renderLanes(SDL_Renderer* ren, double progress) {
    int sw = Config::SCRATCH_WIDTH;
    int totalKeysWidth = 0;
    for (int i = 1; i <= 7; i++) totalKeysWidth += getWidthForLane(i);
    
    int totalWidth = totalKeysWidth + sw; 
    
    int startX = getBaseX(); 
    int laneHeight = 482;
    
    int screenFullWidth = 1280; // Default
    int screenFullHeight = 720; // Default
    SDL_GetRendererOutputSize(ren, &screenFullWidth, &screenFullHeight); 

    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;

    SDL_Rect overallBg = { startX, 0, totalWidth, laneHeight };
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, &overallBg);

    // レーンフレーム描画 (lane_Flame) と 描画済み座標の保持
    int f1DrawW = 0;
    int f1DrawX = 0;
    if (lane_Flame) {
        int imgW, imgH;
        SDL_QueryTexture(lane_Flame, NULL, NULL, &imgW, &imgH);
        
        int imgLanePartW = imgW - 100;
        if (imgLanePartW > 0) {
            double scale = (double)totalWidth / imgLanePartW;
            f1DrawW = (int)(imgW * scale);

            if (Config::PLAY_SIDE == 1) {
                f1DrawX = startX;
                SDL_Rect frameRect = { f1DrawX, 0, f1DrawW, screenFullHeight };
                SDL_RenderCopy(ren, lane_Flame, NULL, &frameRect);
            } else {
                f1DrawX = (startX + totalWidth) - f1DrawW;
                SDL_Rect frameRect = { f1DrawX, 0, f1DrawW, screenFullHeight };
                SDL_RenderCopyEx(ren, lane_Flame, NULL, &frameRect, 0, NULL, SDL_FLIP_HORIZONTAL);
            }
        }
    }

    // レーンフレーム2描画 (lane_Flame2) - 外端からレーンフレーム1の端まで
    if (lane_Flame2) {
        int f2DrawX = 0;
        int f2DrawW = 0;
        if (Config::PLAY_SIDE == 1) {
            // 画面左端(0)から、lane_Flame의右端まで
            f2DrawX = 0;
            f2DrawW = f1DrawX + f1DrawW;
            SDL_Rect frameRect2 = { f2DrawX, 0, f2DrawW, screenFullHeight };
            SDL_RenderCopy(ren, lane_Flame2, NULL, &frameRect2);
        } else {
            // lane_Flameの左端から、画面右端まで
            f2DrawX = f1DrawX;
            f2DrawW = screenFullWidth - f1DrawX;
            SDL_Rect frameRect2 = { f2DrawX, 0, f2DrawW, screenFullHeight };
            SDL_RenderCopyEx(ren, lane_Flame2, NULL, &frameRect2, 0, NULL, SDL_FLIP_HORIZONTAL);
        }
    }

    // スクラッチ画像描画 (tex_scratch)
    if (tex_scratch) {
        // 全てをfloatで計算し、サンプリング精度を向上させる
        float fRadius = ((float)sw * 2.0f) / 3.0f;
        float fDrawSize = fRadius * 2.0f; 
        
        float fDrawX = 0;
        if (Config::PLAY_SIDE == 1) {
            // 1P: 精度の維持のため、startX+swを先に計算してからfloat化して引く
            fDrawX = (float)(startX + sw) - fDrawSize;
        } else {
            // 2P: 鍵盤境界の座標を計算
            fDrawX = (float)(startX + totalWidth - sw);
        }
        
        // 座標指定に float 版の FRect を使用。judgeY も float にキャスト。
        SDL_FRect scRectF = { fDrawX, (float)Config::JUDGMENT_LINE_Y, fDrawSize, fDrawSize };
        // SDL_RenderCopyExF を使用し、サンプリング品質を最高レベルに保つ
        SDL_RenderCopyExF(ren, tex_scratch, NULL, &scRectF, 0.0, NULL, SDL_FLIP_NONE);
    }

    // プログレスバー
    int progX = (Config::PLAY_SIDE == 1) ? startX - 13 : startX + totalWidth + 8;
    int progY = 38, progH = 420, indicatorH = 8;
    SDL_Rect progFrame = { progX, progY, 5, progH };
    SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
    SDL_RenderDrawRect(ren, &progFrame);
    int moveY = progY + (int)((progH - indicatorH) * progress);
    SDL_Rect progIndicator = { progX, moveY, 5, indicatorH };
    SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
    SDL_RenderFillRect(ren, &progIndicator);

    // 各レーンの区切り線
    SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
    for (int i = 1; i <= 8; i++) {
        int lx = getXForLane(i);
        SDL_RenderDrawLine(ren, lx, 0, lx, laneHeight);
    }

    int rightEdge = startX + totalWidth;
    SDL_RenderDrawLine(ren, rightEdge, 0, rightEdge, laneHeight);

    // 鍵盤画像
    if (texKeys) {
        int keyStartX = getXForLane(1);
        int keyEndX = getXForLane(7) + getWidthForLane(7);
        int keyWidth = keyEndX - keyStartX;
        
        int kOrigW, kOrigH; 
        SDL_QueryTexture(texKeys, NULL, NULL, &kOrigW, &kOrigH);
        
        // 1. まずアスペクト比通りに高さを計算
        int keyHeight = (int)(keyWidth * ((float)kOrigH / (float)kOrigW));
        
        // 2. 【ゲージと同様の制限】最大高さを制限する (例: 30px)
        // ゲージの 25px に合わせるなら 25 に、少し厚みが欲しければ 30~40 に調整してください。
        const int MAX_KEY_H = 160; 
        if (keyHeight > MAX_KEY_H) {
            keyHeight = MAX_KEY_H;
        }

        // 3. 描画位置の決定 (判定ライン Config::JUDGMENT_LINE_Y から下に描画)
        // ※LIFTされたライン(judgeY)ではなく、元の判定位置に固定するのが一般的です。
        SDL_Rect keyRect = { keyStartX, Config::JUDGMENT_LINE_Y, keyWidth, keyHeight };
        SDL_RenderCopy(ren, texKeys, NULL, &keyRect);
    }

    // SUDDEN+ / LIFT
    if (Config::SUDDEN_PLUS > 0) {
        int suddenH = std::min(Config::SUDDEN_PLUS, laneHeight);
        SDL_Rect suddenRect = { startX, 0, totalWidth, suddenH };
        if (texLaneCover) {
            int tw, th;
            SDL_QueryTexture(texLaneCover, NULL, NULL, &tw, &th);
            SDL_Rect srcRect = { 0, th - suddenH, tw, suddenH };
            SDL_RenderCopy(ren, texLaneCover, &srcRect, &suddenRect);
        }
        else { 
            SDL_SetRenderDrawColor(ren, 20, 20, 20, 255); 
            SDL_RenderFillRect(ren, &suddenRect); 
        }
    }
    // LIFT の描画: 元の判定位置から、上がった判定ライン(judgeY)までを埋める
    if (Config::LIFT > 0) {
        // 描画先：現在の判定ライン座標(judgeY)から、リフト量(Config::LIFT)分の高さで描画
        SDL_Rect liftRect = { startX, judgeY, totalWidth, Config::LIFT };

        if (texLaneCover) {
            int tw, th;
            SDL_QueryTexture(texLaneCover, NULL, NULL, &tw, &th);
            
            // 【修正ポイント】
            // yを0にすることで、画像の上端(Top)を基準に切り出します。
            // これにより、判定ラインが上がるにつれて、画像の上側から表示されるようになります。
            int srcH = std::min(Config::LIFT, th);
            SDL_Rect srcRect = { 0, 0, tw, srcH };
            
            SDL_RenderCopy(ren, texLaneCover, &srcRect, &liftRect);
        }
        else { 
            SDL_SetRenderDrawColor(ren, 20, 20, 20, 255); 
            SDL_RenderFillRect(ren, &liftRect); 
        }
    }

    // 判定ライン
    SDL_SetRenderDrawColor(ren, 255, 0, 0, 255);
    SDL_RenderDrawLine(ren, startX, judgeY, startX + totalWidth, judgeY);

}

void NoteRenderer::renderNote(SDL_Renderer* ren, const PlayableNote& note, double cur_ms, double speed, bool isAuto) {
    int x = getXForLane(note.lane), w = getWidthForLane(note.lane), judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    double diff = note.target_ms - cur_ms;
    int baseY = judgeY - (int)(diff * speed);
    int y = baseY - 8 - (int)Config::JUDGE_OFFSET;
    int headY = (note.isLN && note.isBeingPressed) ? (judgeY - 8 - (int)Config::JUDGE_OFFSET) : y;
    SDL_Texture *targetTex = nullptr, *lnBodyNormal = nullptr, *lnBodyActive1 = nullptr, *lnBodyActive2 = nullptr, *startTex = nullptr, *endTex = nullptr;
    if (note.lane == 8) { targetTex = texNoteRed; lnBodyNormal = texNoteRed_LN; lnBodyActive1 = texNoteRed_LN_Active1; lnBodyActive2 = texNoteRed_LN_Active2; startTex = texNoteRed_LNS; endTex = texNoteRed_LNE; }
    else if (note.lane % 2 == 0) { targetTex = texNoteBlue; lnBodyNormal = texNoteBlue_LN; lnBodyActive1 = texNoteBlue_LN_Active1; lnBodyActive2 = texNoteBlue_LN_Active2; startTex = texNoteBlue_LNS; endTex = texNoteBlue_LNE; }
    else { targetTex = texNoteWhite; lnBodyNormal = texNoteWhite_LN; lnBodyActive1 = texNoteWhite_LN_Active1; lnBodyActive2 = texNoteWhite_LN_Active2; startTex = texNoteWhite_LNS; endTex = texNoteWhite_LNE; }
    
    if (note.isLN) {
        double endDiff = (note.target_ms + note.duration_ms) - cur_ms;
        int tailY = judgeY - (int)(endDiff * speed) - 8 - (int)Config::JUDGE_OFFSET;
        if (!(tailY > judgeY || headY < Config::SUDDEN_PLUS - 20)) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_Texture* currentBodyTex = lnBodyNormal;
            if (note.isBeingPressed) currentBodyTex = ((SDL_GetTicks() / 100) % 2 == 0) ? lnBodyActive1 : lnBodyActive2;
            int drawTailY = std::max(tailY + 4, Config::SUDDEN_PLUS), drawHeadY = std::min(headY + 4, judgeY);
            if (drawHeadY > drawTailY) {
                SDL_Rect bodyRect = { x + 4, drawTailY, w - 8, drawHeadY - drawTailY };
                if (currentBodyTex) SDL_RenderCopy(ren, currentBodyTex, NULL, &bodyRect);
                else {
                    SDL_SetRenderDrawColor(ren, 180, 180, 180, 150);
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

    // 判定ラインより下でもボムを描画し続けるため、returnではなく条件分岐に変更
    if (!(headY < Config::SUDDEN_PLUS || headY > judgeY)) {
        SDL_Rect rect = { x + 2, headY, w - 4, 8 };
        SDL_Texture* finalHeadTex = (note.isLN && startTex) ? startTex : targetTex;
        if (finalHeadTex) {
            if (isAuto) SDL_SetTextureAlphaMod(finalHeadTex, 160);
            SDL_RenderCopy(ren, finalHeadTex, NULL, &rect);
            if (isAuto) SDL_SetTextureAlphaMod(finalHeadTex, 255);
        }
    }

    // --- ボムの重層描画ロジック ---
    if (note.isLN && note.isBeingPressed && !texBombs.empty()) {
        uint32_t ticks = SDL_GetTicks();
        // 1つ目のボム（200ms周期アニメーション）
        int frame1 = (int)((ticks % 500) * texBombs.size() / 500);
        renderBomb(ren, note.lane, frame1);
        
        // 2つ目のボム（100ms位相をずらして重ねることで、実質100ms間隔の発生に見せる）
        int frame2 = (int)(((ticks + 250) % 500) * texBombs.size() / 500);
        renderBomb(ren, note.lane, frame2);
    }
}

void NoteRenderer::renderBeatLine(SDL_Renderer* ren, double diff, double speed) {
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int y = judgeY - (int)(diff * speed) - (int)Config::JUDGE_OFFSET;
    if (y < Config::SUDDEN_PLUS || y > judgeY) return;
    int totalWidth = 0; for (int i = 1; i <= 7; i++) totalWidth += getWidthForLane(i);
    totalWidth += Config::SCRATCH_WIDTH;
    SDL_SetRenderDrawColor(ren, 60, 60, 70, 255);
    SDL_RenderDrawLine(ren, getBaseX(), y, getBaseX() + totalWidth, y);
}

void NoteRenderer::renderHitEffect(SDL_Renderer* ren, int lane, float progress) {
    int fullW = getWidthForLane(lane), centerX = getXForLane(lane) + fullW / 2, judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int currentW = (int)(fullW * (1.0f - progress));
    if (currentW < 1) return;
    SDL_Texture* targetBeam = (lane == 8) ? texKeybeamRed : (lane % 2 == 0 ? texKeybeamBlue : texKeybeamWhite);
    if (targetBeam) {
        SDL_SetTextureBlendMode(targetBeam, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(targetBeam, (Uint8)((1.0f - progress) * 200));
        SDL_Rect rect = { centerX - currentW / 2, judgeY - 300, currentW, 300 };
        SDL_RenderCopy(ren, targetBeam, NULL, &rect);
    }
}

void NoteRenderer::renderBomb(SDL_Renderer* ren, int lane, int frame) {
    int fullW = getWidthForLane(lane), centerX = getXForLane(lane) + (fullW / 2), judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int size = (int)((Config::LANE_WIDTH * 1.4) * 3.0f); 
    if (!texBombs.empty() && frame >= 0 && frame < (int)texBombs.size()) {
        SDL_Texture* t = texBombs[frame];
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
        SDL_Rect rect = { centerX - (size / 2), judgeY - (size / 2), size, size };
        SDL_RenderCopy(ren, t, NULL, &rect);
    }
}

void NoteRenderer::renderJudgment(SDL_Renderer* ren, const std::string& text, float progress, SDL_Color color, int combo) {
    if (text.empty() || !texJudgeAtlas || !texNumberAtlas) return;
    int judgeType = (text == "P-GREAT") ? 3 : (text == "GREAT" ? 2 : (text == "GOOD" ? 1 : (text == "BAD" ? 0 : 4)));
    int atlasW, atlasH; SDL_QueryTexture(texJudgeAtlas, NULL, NULL, &atlasW, &atlasH);
    int jw = atlasW / 7, jh = atlasH;
    int numAtlasW, numAtlasH; SDL_QueryTexture(texNumberAtlas, NULL, NULL, &numAtlasW, &numAtlasH);
    int nw = numAtlasW / 4, nh = numAtlasH / 10;
    int judgeIdx = 0, colorCol = 3;
    if (judgeType == 3) { int f = (SDL_GetTicks() / 50) % 3; judgeIdx = f; colorCol = f; }
    else if (judgeType == 2) judgeIdx = 3; else if (judgeType == 1) judgeIdx = 4; else if (judgeType == 0) judgeIdx = 5; else judgeIdx = 6;
    float scale = 0.6f; int drawJW = (int)(jw * scale), drawJH = (int)(jh * scale), drawNW = (int)(nw * scale), drawNH = (int)(nh * scale);
    std::string comboStr = (combo > 0) ? std::to_string(combo) : "";
    int totalWidth = drawJW + (comboStr.empty() ? 0 : 20 + (int)comboStr.length() * (drawNW - 10));
    int totalKeysWidth = 0; for (int i = 1; i <= 7; i++) totalKeysWidth += getWidthForLane(i);
    int startX = getBaseX() + (totalKeysWidth + Config::SCRATCH_WIDTH + 10) / 2 - totalWidth / 2;
    int drawY = Config::JUDGMENT_LINE_Y - 170 - Config::LIFT;
    Uint8 alpha = (Uint8)(255 * (1.0f - progress));
    SDL_Rect jSrc = { judgeIdx * jw, 0, jw, jh }, jDst = { startX, drawY, drawJW, drawJH };
    SDL_SetTextureAlphaMod(texJudgeAtlas, alpha); SDL_RenderCopy(ren, texJudgeAtlas, &jSrc, &jDst);
    if (!comboStr.empty()) {
        int curX = startX + drawJW + 20;
        SDL_SetTextureAlphaMod(texNumberAtlas, alpha);
        for (char c : comboStr) {
            SDL_Rect nSrc = { colorCol * nw, (c - '0') * nh, nw, nh }, nDst = { curX, drawY - (drawNH - drawJH) / 2, drawNW, drawNH };
            SDL_RenderCopy(ren, texNumberAtlas, &nSrc, &nDst);
            curX += (drawNW - 10);
        }
    }
}

void NoteRenderer::renderCombo(SDL_Renderer* ren, int combo) {}

void NoteRenderer::renderGauge(SDL_Renderer* ren, double gaugeValue, int gaugeOption, bool isFailed) {
    int totalKeysWidth = 0; for (int i = 1; i <= 7; i++) totalKeysWidth += getWidthForLane(i);
    int totalWidth = totalKeysWidth + Config::SCRATCH_WIDTH + 10;
    int gaugeX = getBaseX();
    
    SDL_Texture* target = nullptr;
    if (!isFailed) {
        if (gaugeOption == 3) target = texGaugeHard; else if (gaugeOption == 4) target = texGaugeExHard;
        else if (gaugeOption == 5) target = texGaugeDan; else if (gaugeOption == 6) target = texGaugeHazard;
        else if (gaugeOption == 1) target = texGaugeAssist; else target = texGaugeNormal;
    }

    int drawGaugeH = 12; 
    if (target) {
        int gOrigW, gOrigH; SDL_QueryTexture(target, NULL, NULL, &gOrigW, &gOrigH);
        drawGaugeH = (int)(totalWidth * ((float)gOrigH / (float)gOrigW));
        if (drawGaugeH > 25) drawGaugeH = 25;
    }

    int gaugeBottomY = Config::SCREEN_HEIGHT - 40; 
    int gaugeY = gaugeBottomY - drawGaugeH;

    std::string framePath = Config::ROOT_PATH + "Skin/gauge_frame.png";
    if (textureCache.find(framePath) == textureCache.end()) {
        SDL_Texture* t = IMG_LoadTexture(ren, framePath.c_str());
        if (t) textureCache[framePath] = t;
    }
    SDL_Texture* frameTex = textureCache.count(framePath) ? textureCache[framePath] : nullptr;

    if (target) {
        int gOrigW, gOrigH; SDL_QueryTexture(target, NULL, NULL, &gOrigW, &gOrigH);
        if (frameTex) {
            int fOrigW, fOrigH; SDL_QueryTexture(frameTex, NULL, NULL, &fOrigW, &fOrigH);
            float widthRatio = (float)fOrigW / (float)gOrigW;
            float heightRatio = (float)fOrigH / (float)gOrigH;
            int drawFW = (int)(totalWidth * widthRatio);
            int drawFH = (int)(drawGaugeH * heightRatio);
            int frameX = (gaugeX + totalWidth / 2) - (drawFW / 2);
            int frameY = (gaugeY + drawGaugeH / 2) - (drawFH / 2);
            SDL_Rect fRect = { frameX, frameY, drawFW, drawFH };
            SDL_RenderCopyEx(ren, frameTex, NULL, &fRect, 0, NULL, (Config::PLAY_SIDE == 1) ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
        }

        // --- 背景描画 (薄いゲージ画像) ---
        SDL_SetTextureAlphaMod(target, 60);
        SDL_Rect bgRect = { gaugeX, gaugeY, totalWidth, drawGaugeH };
        SDL_RenderCopyEx(ren, target, NULL, &bgRect, 0, NULL, (Config::PLAY_SIDE == 1) ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
        SDL_SetTextureAlphaMod(target, 255);

        // --- セグメント描画 (先端固定 + 手前揺れ) ---
        float segW = (float)totalWidth / 50.0f, srcSegW = (float)gOrigW / 50.0f;
        
        int displayValInt = (int)gaugeValue;
        if (displayValInt > 100) displayValInt = 100;
        if (displayValInt < 0) displayValInt = 0;
        int finalNum = (Config::GAUGE_DISPLAY_TYPE == 1) ? (displayValInt / 2) * 2 : displayValInt;

        int activeSegments = finalNum / 2;

        for (int i = 0; i < 50; i++) {
            bool draw = false;
            if (i < activeSegments) {
                // 先端（最後のセグメント）は必ず描画する
                if (i == activeSegments - 1) {
                    draw = true;
                } else {
                    // それより手前のセグメントを揺らす演出
                    draw = (i < activeSegments - 4) || 
                           (i < activeSegments - 1 && rand() % 100 < 50) || 
                           (i >= activeSegments - 4 && (SDL_GetTicks() / 60) % 2 == 0);
                }
            }

            if (draw) {
                int curPos = (int)(i * segW), nextPos = (int)((i + 1) * segW);
                int dx = (Config::PLAY_SIDE == 1) ? (gaugeX + curPos) : (gaugeX + totalWidth - nextPos);
                SDL_Rect dR = { dx, gaugeY, nextPos - curPos, drawGaugeH };
                SDL_Rect sR = { (int)(i * srcSegW), 0, (int)((i + 1) * srcSegW) - (int)(i * srcSegW), gOrigH };
                SDL_RenderCopyEx(ren, target, &sR, &dR, 0, NULL, (Config::PLAY_SIDE == 1) ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
            }
        }
    } else if (isFailed) {
        SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        SDL_Rect failRect = { gaugeX, gaugeY, totalWidth, drawGaugeH };
        SDL_RenderFillRect(ren, &failRect);
    }

//    char gStr[16]; 
//    int finalNumForText = (int)gaugeValue;
//    if (finalNumForText > 100) finalNumForText = 100;
//    if (finalNumForText < 0) finalNumForText = 0;

//    if (Config::GAUGE_DISPLAY_TYPE == 1) {
//        finalNumForText = (finalNumForText / 2) * 2;
//        snprintf(gStr, sizeof(gStr), "%3d%%", finalNumForText);
//    } else {
//        snprintf(gStr, sizeof(gStr), "%5.1f%%", gaugeValue);
//    }
    
//    drawTextCached(ren, gStr, (Config::PLAY_SIDE == 1) ? (gaugeX + totalWidth - 65) : gaugeX, gaugeY + drawGaugeH + 5, {255, 255, 255, 255}, false, false);
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
