#include "NoteRenderer.hpp"
#include "Config.hpp"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <SDL2/SDL_image.h>

// --- レーンレイアウトキャッシュ再構築 ---
// Config の値は init() 後に変化しないため、ここで一度だけ計算する。
// 描画ループ内での getXForLane / getWidthForLane の都度計算を排除する。
void NoteRenderer::rebuildLaneLayout() {
    // 各レーン幅
    for (int i = 1; i <= 7; i++)
        ll.w[i] = (i % 2 != 0) ? (int)(Config::LANE_WIDTH * 1.4) : Config::LANE_WIDTH;
    ll.w[8] = Config::SCRATCH_WIDTH;

    int keysWidth = 0;
    for (int i = 1; i <= 7; i++) keysWidth += ll.w[i];
    ll.totalWidth = keysWidth + Config::SCRATCH_WIDTH;

    ll.baseX = (Config::PLAY_SIDE == 1)
        ? 50
        : (Config::SCREEN_WIDTH - ll.totalWidth - 50);

    // 各レーン X 座標
    if (Config::PLAY_SIDE == 1) {
        ll.x[8] = ll.baseX;                     // スクラッチ左端
        int cur = ll.baseX + Config::SCRATCH_WIDTH;
        for (int i = 1; i <= 7; i++) { ll.x[i] = cur; cur += ll.w[i]; }
    } else {
        int cur = ll.baseX;
        for (int i = 1; i <= 7; i++) { ll.x[i] = cur; cur += ll.w[i]; }
        ll.x[8] = cur;                           // スクラッチ右端
    }

    // BGA 表示中心 X
    if (Config::PLAY_SIDE == 1) {
        int right = ll.baseX + ll.totalWidth;
        ll.bgaCenterX = right + (Config::SCREEN_WIDTH - right) / 2;
    } else {
        ll.bgaCenterX = ll.baseX / 2;
    }
}

// ============================================================
//  NoteRenderer 実装
// ============================================================

void NoteRenderer::loadAndCache(SDL_Renderer* ren, TextureRegion& region, const std::string& path) {
    region.reset();
    SDL_Surface* s = IMG_Load(path.c_str());
    if (s) {
        region.texture = SDL_CreateTextureFromSurface(ren, s);
        region.w = s->w;
        region.h = s->h;
        SDL_FreeSurface(s);
    } else {
        printf("NoteRenderer::loadAndCache failed: %s\n", path.c_str());
    }
}

void NoteRenderer::init(SDL_Renderer* ren) {
    TTF_Init();
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    fontSmall = TTF_OpenFont(Config::FONT_PATH.c_str(), 24);
    fontBig   = TTF_OpenFont(Config::FONT_PATH.c_str(), 48);

    std::string s = Config::ROOT_PATH + "Skin/";

    loadAndCache(ren, texBackground, s + "Flame_BG.png");
    loadAndCache(ren, texNoteWhite,  s + "note_white.png");
    loadAndCache(ren, texNoteBlue,   s + "note_blue.png");
    loadAndCache(ren, texNoteRed,    s + "note_red.png");

    loadAndCache(ren, texNoteWhite_LN,       s + "note_white_ln.png");
    loadAndCache(ren, texNoteWhite_LN_Active1, s + "note_white_ln_active1.png");
    loadAndCache(ren, texNoteWhite_LN_Active2, s + "note_white_ln_active2.png");
    loadAndCache(ren, texNoteBlue_LN,        s + "note_blue_ln.png");
    loadAndCache(ren, texNoteBlue_LN_Active1,  s + "note_blue_ln_active1.png");
    loadAndCache(ren, texNoteBlue_LN_Active2,  s + "note_blue_ln_active2.png");
    loadAndCache(ren, texNoteRed_LN,         s + "note_red_ln.png");
    loadAndCache(ren, texNoteRed_LN_Active1,   s + "note_red_ln_active1.png");
    loadAndCache(ren, texNoteRed_LN_Active2,   s + "note_red_ln_active2.png");

    loadAndCache(ren, texNoteWhite_LNS, s + "note_white_lns.png");
    loadAndCache(ren, texNoteWhite_LNE, s + "note_white_lne.png");
    loadAndCache(ren, texNoteBlue_LNS,  s + "note_blue_lns.png");
    loadAndCache(ren, texNoteBlue_LNE,  s + "note_blue_lne.png");
    loadAndCache(ren, texNoteRed_LNS,   s + "note_red_lns.png");
    loadAndCache(ren, texNoteRed_LNE,   s + "note_red_lne.png");

    loadAndCache(ren, texKeybeamWhite, s + "beam_white.png");
    loadAndCache(ren, texKeybeamBlue,  s + "beam_blue.png");
    loadAndCache(ren, texKeybeamRed,   s + "beam_red.png");

    loadAndCache(ren, texJudgeAtlas,  s + "judge.png");
    loadAndCache(ren, texNumberAtlas, s + "judge_number.png");
    loadAndCache(ren, texLaneCover,   s + "lanecover.png");

    loadAndCache(ren, texGaugeAssist, s + "gauge_assist.png");
    loadAndCache(ren, texGaugeNormal, s + "gauge_normal.png");
    loadAndCache(ren, texGaugeHard,   s + "gauge_hard.png");
    loadAndCache(ren, texGaugeExHard, s + "gauge_exhard.png");
    loadAndCache(ren, texGaugeHazard, s + "gauge_hazard.png");
    loadAndCache(ren, texGaugeDan,    s + "gauge_dan.png");

    loadAndCache(ren, texKeys,      s + "7keypad.png");
    loadAndCache(ren, lane_Flame,   s + "lane_Flame.png");
    loadAndCache(ren, lane_Flame2,  s + "lane_Flame2.png");

    texBombs.clear();
    for (int i = 0; i < 10; i++) {
        TextureRegion tr;
        loadAndCache(ren, tr, s + "bomb_" + std::to_string(i) + ".png");
        if (tr) texBombs.push_back(tr);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    loadAndCache(ren, tex_scratch, s + "scratch.png");
    if (tex_scratch) {
        SDL_SetTextureScaleMode(tex_scratch.texture, SDL_ScaleModeBest);
        SDL_SetTextureBlendMode(tex_scratch.texture, SDL_BLENDMODE_BLEND);
    }
    loadAndCache(ren, tex_scratch_center, s + "scratch_center.png");
    if (tex_scratch_center) {
        SDL_SetTextureScaleMode(tex_scratch_center.texture, SDL_ScaleModeBest);
        SDL_SetTextureBlendMode(tex_scratch_center.texture, SDL_BLENDMODE_BLEND);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    rebuildLaneLayout();
}

void NoteRenderer::cleanup() {
    clearTextCache();
    if (fontSmall) TTF_CloseFont(fontSmall);
    if (fontBig)   TTF_CloseFont(fontBig);

    texBackground.reset();
    texNoteWhite.reset(); texNoteBlue.reset(); texNoteRed.reset();
    texNoteWhite_LN.reset(); texNoteWhite_LN_Active1.reset(); texNoteWhite_LN_Active2.reset();
    texNoteBlue_LN.reset();  texNoteBlue_LN_Active1.reset();  texNoteBlue_LN_Active2.reset();
    texNoteRed_LN.reset();   texNoteRed_LN_Active1.reset();   texNoteRed_LN_Active2.reset();
    texNoteWhite_LNS.reset(); texNoteWhite_LNE.reset();
    texNoteBlue_LNS.reset();  texNoteBlue_LNE.reset();
    texNoteRed_LNS.reset();   texNoteRed_LNE.reset();
    texKeybeamWhite.reset(); texKeybeamBlue.reset(); texKeybeamRed.reset();
    texJudgeAtlas.reset(); texNumberAtlas.reset(); texLaneCover.reset();
    texGaugeAssist.reset(); texGaugeNormal.reset(); texGaugeHard.reset();
    texGaugeExHard.reset(); texGaugeHazard.reset(); texGaugeDan.reset();
    texKeys.reset();
    lane_Flame.reset(); lane_Flame2.reset();
    tex_scratch.reset(); tex_scratch_center.reset();

    for (auto& b : texBombs) b.reset();
    texBombs.clear();

    for (auto& pair : textureCache) pair.second.reset();
    textureCache.clear();

    for (auto& pair : customFontCache) if (pair.second) TTF_CloseFont(pair.second);
    customFontCache.clear();

    IMG_Quit();
    TTF_Quit();
}

void NoteRenderer::renderBackground(SDL_Renderer* ren) {
    if (texBackground) {
        if (Config::PLAY_SIDE == 1) {
            SDL_RenderCopy(ren, texBackground.texture, NULL, NULL);
        } else {
            SDL_RenderCopyEx(ren, texBackground.texture, NULL, NULL, 0, NULL, SDL_FLIP_HORIZONTAL);
        }
    }
}

// drawText: ローディング画面など毎フレーム内容が変わる箇所専用。
// ゲームループ内で固定テキストに使うことは厳禁。
void NoteRenderer::drawText(SDL_Renderer* ren, const std::string& text, int x, int y,
                             SDL_Color color, bool isBig, bool isCenter, bool isRight,
                             const std::string& fontPath) {
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
        if (isCenter)    drawX = x - s->w / 2;
        else if (isRight) drawX = x - s->w;
        SDL_Rect dst = { drawX, y, s->w, s->h };
        SDL_RenderCopy(ren, t, NULL, &dst);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

void NoteRenderer::drawTextCached(SDL_Renderer* ren, const std::string& text, int x, int y,
                                   SDL_Color color, bool isBig, bool isCenter, bool isRight,
                                   const std::string& fontPath) {
    if (text.empty()) return;
    uint32_t rgba = (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
    TextCacheKey key = { text, rgba, isBig, fontPath };

    auto it = textTextureCache.find(key);
    if (it != textTextureCache.end()) {
        lruList.erase(it->second.lruIt);
        lruList.push_front(key);
        it->second.lruIt = lruList.begin();
        int drawX = x;
        if (isCenter)    drawX = x - it->second.w / 2;
        else if (isRight) drawX = x - it->second.w;
        SDL_Rect dst = { drawX, y, it->second.w, it->second.h };
        SDL_RenderCopy(ren, it->second.texture, NULL, &dst);
    } else {
        if (textTextureCache.size() >= MAX_TEXT_CACHE) {
            TextCacheKey oldestKey = lruList.back();
            SDL_DestroyTexture(textTextureCache[oldestKey].texture);
            textTextureCache.erase(oldestKey);
            lruList.pop_back();
        }
        TTF_Font* targetFont = isBig ? fontBig : fontSmall;
        if (!fontPath.empty() && customFontCache.count(fontPath)) targetFont = customFontCache[fontPath];
        if (!targetFont) return;
        SDL_Surface* s = TTF_RenderUTF8_Blended(targetFont, text.c_str(), color);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
            if (t) {
                lruList.push_front(key);
                textTextureCache[key] = { t, s->w, s->h, lruList.begin() };
                int drawX = x;
                if (isCenter)    drawX = x - s->w / 2;
                else if (isRight) drawX = x - s->w;
                SDL_Rect dst = { drawX, y, s->w, s->h };
                SDL_RenderCopy(ren, t, NULL, &dst);
            }
            SDL_FreeSurface(s);
        }
    }
}

void NoteRenderer::clearTextCache() {
    for (auto& pair : textTextureCache) if (pair.second.texture) SDL_DestroyTexture(pair.second.texture);
    textTextureCache.clear();
    lruList.clear();
}

void NoteRenderer::drawImage(SDL_Renderer* ren, const std::string& path,
                              int x, int y, int w, int h, int alpha) {
    if (path.empty()) return;
    if (textureCache.find(path) == textureCache.end()) {
        loadAndCache(ren, textureCache[path], path);
    }
    TextureRegion& tr = textureCache[path];
    if (tr) {
        SDL_SetTextureAlphaMod(tr.texture, (Uint8)alpha);
        SDL_Rect dst = { x, y, w, h };
        SDL_RenderCopy(ren, tr.texture, NULL, &dst);
    }
}

void NoteRenderer::renderDecisionInfo(SDL_Renderer* ren, const BMSHeader& header) {
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color gray   = {180, 180, 180, 255};
    SDL_Color yellow = {255, 255, 0, 255};
    int centerX = 640;
    drawTextCached(ren, header.genre,   centerX, 220, gray, false, true);
    drawTextCached(ren, header.title,   centerX, 270, white, true, true);
    drawTextCached(ren, header.artist,  centerX, 340, white, false, true);
    std::string levelInfo = "[" + header.chartName + "]  LEVEL " + std::to_string(header.level);
    drawTextCached(ren, levelInfo, centerX, 375, yellow, false, true);
}

void NoteRenderer::renderUI(SDL_Renderer* ren, const BMSHeader& header, int fps, double bpm, int exScore) {
    int centerX = ll.bgaCenterX;
    std::string platePath = Config::ROOT_PATH + "Skin/Flame_nameplate.png";
    if (textureCache.find(platePath) == textureCache.end()) {
        loadAndCache(ren, textureCache[platePath], platePath);
    }
    TextureRegion& tr = textureCache[platePath];
    if (tr) {
        SDL_Rect dst = { centerX - tr.w / 2, 0, tr.w, tr.h };
        SDL_RenderCopy(ren, tr.texture, NULL, &dst);
    }
}

// スクラッチ回転アニメーション用状態（ファイルスコープで慣性を保持）
static double s_scratchAngle = 0.0;
static double s_scratchSpeed = 0.0;

void NoteRenderer::renderLanes(SDL_Renderer* ren, double progress, int scratchStatus) {
    int totalWidth = ll.totalWidth;
    int startX     = ll.baseX;
    int laneHeight = 482;
    int judgeY     = Config::JUDGMENT_LINE_Y - Config::LIFT;

    SDL_Rect overallBg = { startX, 0, totalWidth, laneHeight };
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderFillRect(ren, &overallBg);

    if (lane_Flame) {
        int imgLanePartW = lane_Flame.w - 100;
        if (imgLanePartW > 0) {
            double scale = (double)totalWidth / imgLanePartW;
            int f1W = (int)(lane_Flame.w * scale);
            int f1X = (Config::PLAY_SIDE == 1) ? startX : (startX + totalWidth) - f1W;
            SDL_Rect r = { f1X, 0, f1W, Config::SCREEN_HEIGHT };
            SDL_RenderCopyEx(ren, lane_Flame.texture, NULL, &r, 0, NULL,
                             (Config::PLAY_SIDE == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL));
        }
    }

    if (tex_scratch) {
        float fSize = ((float)Config::SCRATCH_WIDTH * 2.0f / 3.0f) * 2.0f;
        float fX = (Config::PLAY_SIDE == 1)
            ? (float)(startX + Config::SCRATCH_WIDTH) - fSize
            : (float)(startX + totalWidth - Config::SCRATCH_WIDTH);
        SDL_FRect rF = { fX, (float)Config::JUDGMENT_LINE_Y, fSize, fSize };
        SDL_RenderCopyExF(ren, tex_scratch.texture, NULL, &rF, 0.0, NULL, SDL_FLIP_NONE);

        if (tex_scratch_center) {
            // 慣性付き回転：入力に応じて目標速度に追従
            double targetSpeed = (scratchStatus == 1) ? -5.0 : (scratchStatus == 2) ? 15.0 : 0.0;
            s_scratchSpeed += (targetSpeed - s_scratchSpeed) * 0.1;
            s_scratchAngle += s_scratchSpeed;
            if (s_scratchAngle >= 360.0) s_scratchAngle -= 360.0;
            if (s_scratchAngle <    0.0) s_scratchAngle += 360.0;
            SDL_RenderCopyExF(ren, tex_scratch_center.texture, NULL, &rF, s_scratchAngle, NULL, SDL_FLIP_NONE);
        }
    }

    if (texKeys) {
        int kX = ll.x[1], kEnd = ll.x[7] + ll.w[7];
        int kW = kEnd - kX;
        int kH = std::min(160, (int)(kW * ((float)texKeys.h / texKeys.w)));
        SDL_Rect r = { kX, Config::JUDGMENT_LINE_Y, kW, kH };
        SDL_RenderCopy(ren, texKeys.texture, NULL, &r);
    }

    if (Config::SUDDEN_PLUS > 0) {
        int sH = std::min(Config::SUDDEN_PLUS, laneHeight);
        SDL_Rect dR = { startX, 0, totalWidth, sH };
        if (texLaneCover) {
            SDL_Rect sR = { 0, texLaneCover.h - sH, texLaneCover.w, sH };
            SDL_RenderCopy(ren, texLaneCover.texture, &sR, &dR);
        } else {
            SDL_SetRenderDrawColor(ren, 20, 20, 20, 255); SDL_RenderFillRect(ren, &dR);
        }
    }
    if (Config::LIFT > 0) {
        SDL_Rect dR = { startX, judgeY, totalWidth, Config::LIFT };
        if (texLaneCover) {
            SDL_Rect sR = { 0, 0, texLaneCover.w, std::min(Config::LIFT, texLaneCover.h) };
            SDL_RenderCopy(ren, texLaneCover.texture, &sR, &dR);
        } else {
            SDL_SetRenderDrawColor(ren, 20, 20, 20, 255); SDL_RenderFillRect(ren, &dR);
        }
    }
    SDL_SetRenderDrawColor(ren, 255, 0, 0, 255);
    SDL_RenderDrawLine(ren, startX, judgeY, startX + totalWidth, judgeY);
}

void NoteRenderer::renderNote(SDL_Renderer* ren, const PlayableNote& note,
                               double cur_ms, double speed, bool isAuto) {
    int x = ll.x[note.lane], w = ll.w[note.lane];
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int headY  = judgeY - (int)((note.target_ms - cur_ms) * speed) - 8 - (int)Config::JUDGE_OFFSET;
    if (note.isLN && note.isBeingPressed) headY = judgeY - 8 - (int)Config::JUDGE_OFFSET;

    const TextureRegion *target = nullptr, *lnB = nullptr, *lnA1 = nullptr, *lnA2 = nullptr, *lnS = nullptr, *lnE = nullptr;
    if (note.lane == 8) {
        target = &texNoteRed; lnB = &texNoteRed_LN;
        lnA1 = &texNoteRed_LN_Active1; lnA2 = &texNoteRed_LN_Active2;
        lnS  = &texNoteRed_LNS;        lnE  = &texNoteRed_LNE;
    } else if (note.lane % 2 == 0) {
        target = &texNoteBlue; lnB = &texNoteBlue_LN;
        lnA1 = &texNoteBlue_LN_Active1; lnA2 = &texNoteBlue_LN_Active2;
        lnS  = &texNoteBlue_LNS;        lnE  = &texNoteBlue_LNE;
    } else {
        target = &texNoteWhite; lnB = &texNoteWhite_LN;
        lnA1 = &texNoteWhite_LN_Active1; lnA2 = &texNoteWhite_LN_Active2;
        lnS  = &texNoteWhite_LNS;        lnE  = &texNoteWhite_LNE;
    }

    if (note.isLN) {
        int tailY = judgeY - (int)(((note.target_ms + note.duration_ms) - cur_ms) * speed)
                    - 8 - (int)Config::JUDGE_OFFSET;
        if (!(tailY > judgeY || headY < Config::SUDDEN_PLUS - 20)) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            const TextureRegion* body = note.isBeingPressed
                ? ((SDL_GetTicks() / 100 % 2 == 0) ? lnA1 : lnA2)
                : lnB;
            int dTY = std::max(tailY + 4, (int)Config::SUDDEN_PLUS);
            int dHY = std::min(headY + 4, judgeY);
            if (dHY > dTY && body && *body) {
                SDL_Rect r = { x + 4, dTY, w - 8, dHY - dTY };
                SDL_RenderCopy(ren, body->texture, NULL, &r);
            }
            if (tailY >= Config::SUDDEN_PLUS && tailY <= judgeY) {
                const TextureRegion* end = (lnE && *lnE) ? lnE : target;
                if (end && *end) {
                    SDL_Rect r = { x + 2, tailY, w - 4, end->h };
                    SDL_RenderCopy(ren, end->texture, NULL, &r);
                }
            }
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }
    }
    if (!(headY < Config::SUDDEN_PLUS || headY > judgeY)) {
        const TextureRegion* head = (note.isLN && lnS && *lnS) ? lnS : target;
        if (head && *head) {
            SDL_Rect r = { x + 2, headY, w - 4, head->h };
            if (isAuto) SDL_SetTextureAlphaMod(head->texture, 160);
            SDL_RenderCopy(ren, head->texture, NULL, &r);
            if (isAuto) SDL_SetTextureAlphaMod(head->texture, 255);
        }
    }
}

void NoteRenderer::renderBeatLine(SDL_Renderer* ren, double diff, double speed) {
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int y      = judgeY - (int)(diff * speed) - (int)Config::JUDGE_OFFSET;
    if (y < Config::SUDDEN_PLUS || y > judgeY) return;
    SDL_SetRenderDrawColor(ren, 60, 60, 70, 255);
    SDL_RenderDrawLine(ren, ll.baseX, y, ll.baseX + ll.totalWidth, y);
}

void NoteRenderer::renderHitEffect(SDL_Renderer* ren, int lane, float progress) {
    int fullW  = ll.w[lane];
    int cX     = ll.x[lane] + fullW / 2;
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int curW   = (int)(fullW * (1.0f - progress));
    if (curW < 1) return;
    const TextureRegion* beam = (lane == 8) ? &texKeybeamRed
                              : (lane % 2 == 0 ? &texKeybeamBlue : &texKeybeamWhite);
    if (beam && *beam) {
        SDL_SetTextureBlendMode(beam->texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(beam->texture, (Uint8)((1.0f - progress) * 200));
        SDL_Rect r = { cX - curW / 2, judgeY - 300, curW, 300 };
        SDL_RenderCopy(ren, beam->texture, NULL, &r);
    }
}

void NoteRenderer::renderBomb(SDL_Renderer* ren, int lane, int frame) {
    if (texBombs.empty() || frame < 0 || frame >= (int)texBombs.size()) return;
    int fullW  = ll.w[lane];
    int cX     = ll.x[lane] + (fullW / 2);
    int judgeY = Config::JUDGMENT_LINE_Y - Config::LIFT;
    int size   = (int)((Config::LANE_WIDTH * 1.4) * 3.0f);
    const TextureRegion& tr = texBombs[frame];
    if (tr) {
        SDL_SetTextureBlendMode(tr.texture, SDL_BLENDMODE_ADD);
        SDL_Rect r = { cX - size / 2, judgeY - size / 2, size, size };
        SDL_RenderCopy(ren, tr.texture, NULL, &r);
    }
}

// ★修正：JudgeKind ベースのオーバーロード。文字列比較ループを廃止。
void NoteRenderer::renderJudgment(SDL_Renderer* ren, JudgeKind kind, float progress, int combo) {
    if (kind == JudgeKind::NONE || !texJudgeAtlas || !texNumberAtlas) return;

    // JudgeKind → アトラスインデックス
    int type = 4; // POOR
    switch (kind) {
        case JudgeKind::PGREAT: type = 3; break;
        case JudgeKind::GREAT:  type = 2; break;
        case JudgeKind::GOOD:   type = 1; break;
        case JudgeKind::BAD:    type = 0; break;
        default: break;
    }

    int jw = texJudgeAtlas.w / 7, jh = texJudgeAtlas.h;
    int nw = texNumberAtlas.w / 4, nh = texNumberAtlas.h / 10;
    int jIdx = (type == 3) ? (SDL_GetTicks() / 50 % 3)
             : (type == 2 ? 3 : (type == 1 ? 4 : (type == 0 ? 5 : 6)));
    float s = 0.6f;
    int dJW = (int)(jw * s), dJH = (int)(jh * s);
    int dNW = (int)(nw * s), dNH = (int)(nh * s);

    // combo を文字列化（snprintf でスタック上に確保、heap alloc なし）
    char comboStr[16] = {};
    int  comboLen = 0;
    if (combo > 0) {
        comboLen = snprintf(comboStr, sizeof(comboStr), "%d", combo);
    }

    int tW = dJW + (comboLen > 0 ? 20 + comboLen * (dNW - 10) : 0);
    int sX = ll.baseX + (ll.totalWidth + 10) / 2 - tW / 2;
    int dY = Config::JUDGMENT_LINE_Y - 170 - Config::LIFT;
    Uint8 alpha = (Uint8)(255 * (1.0f - progress));

    SDL_Rect jS = { jIdx * jw, 0, jw, jh }, jD = { sX, dY, dJW, dJH };
    SDL_SetTextureAlphaMod(texJudgeAtlas.texture, alpha);
    SDL_RenderCopy(ren, texJudgeAtlas.texture, &jS, &jD);

    if (comboLen > 0) {
        int curX = sX + dJW + 20;
        SDL_SetTextureAlphaMod(texNumberAtlas.texture, alpha);
        int colorIdx = (type == 3) ? (SDL_GetTicks() / 50 % 3) : 3;
        for (int ci = 0; ci < comboLen; ++ci) {
            int digit = comboStr[ci] - '0';
            SDL_Rect nS = { colorIdx * nw, digit * nh, nw, nh };
            SDL_Rect nD = { curX, dY - (dNH - dJH) / 2, dNW, dNH };
            SDL_RenderCopy(ren, texNumberAtlas.texture, &nS, &nD);
            curX += (dNW - 10);
        }
    }
}

// 後方互換用オーバーロード（旧 std::string 版）
void NoteRenderer::renderJudgment(SDL_Renderer* ren, const std::string& text,
                                   float progress, SDL_Color /*color*/, int combo) {
    JudgeKind kind = JudgeKind::POOR;
    if      (text == "P-GREAT") kind = JudgeKind::PGREAT;
    else if (text == "GREAT")   kind = JudgeKind::GREAT;
    else if (text == "GOOD")    kind = JudgeKind::GOOD;
    else if (text == "BAD")     kind = JudgeKind::BAD;
    renderJudgment(ren, kind, progress, combo);
}

void NoteRenderer::renderCombo(SDL_Renderer* /*ren*/, int /*combo*/) {
    // 未実装
}

void NoteRenderer::renderGauge(SDL_Renderer* ren, double gaugeValue, int gaugeOption, bool isFailed) {
    int totalW = ll.totalWidth + 10;
    int gX = ll.baseX;

    const TextureRegion* target = nullptr;
    if (!isFailed) {
        if (gaugeOption == 3)      target = &texGaugeHard;
        else if (gaugeOption == 4) target = &texGaugeExHard;
        else if (gaugeOption == 5) target = &texGaugeDan;
        else if (gaugeOption == 6) target = &texGaugeHazard;
        else if (gaugeOption == 1) target = &texGaugeAssist;
        else                       target = &texGaugeNormal;
    }

    int dGH = 12;
    if (target && *target)
        dGH = std::min(25, (int)(totalW * ((float)target->h / target->w)));
    int gY = (Config::SCREEN_HEIGHT - 40) - dGH;

    std::string fPath = Config::ROOT_PATH + "Skin/gauge_frame.png";
    if (textureCache.find(fPath) == textureCache.end()) loadAndCache(ren, textureCache[fPath], fPath);
    TextureRegion* fTR = textureCache.count(fPath) ? &textureCache[fPath] : nullptr;

    if (target && *target) {
        if (fTR && *fTR) {
            float wR = (float)fTR->w / target->w, hR = (float)fTR->h / target->h;
            int dFW = (int)(totalW * wR), dFH = (int)(dGH * hR);
            SDL_Rect r = { (gX + totalW / 2) - (dFW / 2), (gY + dGH / 2) - (dFH / 2), dFW, dFH };
            SDL_RenderCopyEx(ren, fTR->texture, NULL, &r, 0, NULL,
                             (Config::PLAY_SIDE == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL));
        }
        SDL_SetTextureAlphaMod(target->texture, 60);
        SDL_Rect bgR = { gX, gY, totalW, dGH };
        SDL_RenderCopyEx(ren, target->texture, NULL, &bgR, 0, NULL,
                         (Config::PLAY_SIDE == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL));
        SDL_SetTextureAlphaMod(target->texture, 255);

        float segW  = (float)totalW / 50.0f;
        float sSegW = (float)target->w / 50.0f;
        // GAUGE_DISPLAY_TYPE==1: 2刻み偶数表示、それ以外: そのまま
        int displayVal = std::clamp((int)gaugeValue, 0, 100);
        int activeS = (Config::GAUGE_DISPLAY_TYPE == 1) ? ((displayVal / 2) * 2) / 2 : displayVal / 2;
        for (int i = 0; i < 50; i++) {
            if (i < activeS && (i == activeS - 1 || i < activeS - 4
                || rand() % 100 < 50 || (SDL_GetTicks() / 60) % 2 == 0)) {
                int cP = (int)(i * segW), nP = (int)((i + 1) * segW);
                int dx = (Config::PLAY_SIDE == 1) ? (gX + cP) : (gX + totalW - nP);
                SDL_Rect dR = { dx, gY, nP - cP, dGH };
                SDL_Rect sR = { (int)(i * sSegW), 0,
                                (int)((i + 1) * sSegW) - (int)(i * sSegW), target->h };
                SDL_RenderCopyEx(ren, target->texture, &sR, &dR, 0, NULL,
                                 (Config::PLAY_SIDE == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL));
            }
        }
    } else if (isFailed) {
        SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
        SDL_Rect r = { gX, gY, totalW, dGH };
        SDL_RenderFillRect(ren, &r);
    }
}

void NoteRenderer::renderLoading(SDL_Renderer* ren, int current, int total, const std::string& filename) {
    SDL_SetRenderDrawColor(ren, 0, 0, 5, 255); SDL_RenderClear(ren);
    // ローディング画面は drawText で構わない（描画頻度が低く、内容も変わる）
    drawText(ren, "NOW LOADING...", 640, 300, {255, 255, 255, 255}, true, true);
    SDL_Rect bO = { 100, 380, 1080, 20 };
    SDL_SetRenderDrawColor(ren, 40, 40, 40, 255); SDL_RenderDrawRect(ren, &bO);
    float p = (total > 0) ? (float)current / total : 0;
    SDL_Rect bI = { 102, 382, (int)(1076 * p), 16 };
    SDL_SetRenderDrawColor(ren, 0, 120, 255, 255); SDL_RenderFillRect(ren, &bI);
    drawText(ren, filename, 640, 420, {150, 150, 150, 255}, false, true);
}

void NoteRenderer::renderResult(SDL_Renderer* ren, const PlayStatus& status,
                                 const BMSHeader& header, const std::string& rank) {
    SDL_SetRenderDrawColor(ren, 5, 5, 10, 255); SDL_RenderClear(ren);
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 255,   0, 255};
    // ★修正：リザルト画面の固定テキストは drawTextCached を使用
    drawTextCached(ren, header.title,       640, 50,  white, true, true);
    drawTextCached(ren, "RANK: " + rank,    640, 120, yellow, true, true);
    int sY = 240, sp = 45;
    // 数値は毎回変わらないので drawText でもよいが、リザルトはフレーム数が少ないため許容範囲
    drawText(ren, "P-GREAT : " + std::to_string(status.pGreatCount), 400, sY,         white, false);
    drawText(ren, "GREAT   : " + std::to_string(status.greatCount),  400, sY + sp,    white, false);
    drawText(ren, "GOOD    : " + std::to_string(status.goodCount),   400, sY + sp*2,  white, false);
    drawText(ren, "BAD     : " + std::to_string(status.badCount),    400, sY + sp*3,  white, false);
    drawText(ren, "POOR    : " + std::to_string(status.poorCount),   400, sY + sp*4,  white, false);
    drawText(ren, "MAX COMBO : " + std::to_string(status.maxCombo),  680, sY,         yellow, false);
    drawText(ren, "EX SCORE  : " + std::to_string((status.pGreatCount * 2) + status.greatCount),
                  680, sY + sp, white, false);

    // クリアタイプ表示
    std::string clearText  = "FAILED";
    SDL_Color   clearColor = {100, 100, 100, 255};
    switch (status.clearType) {
        case ClearType::FULL_COMBO:    clearText = "FULL COMBO";    clearColor = {255, 255, 255, 255}; break;
        case ClearType::EX_HARD_CLEAR: clearText = "EX-HARD CLEAR"; clearColor = {255, 255,   0, 255}; break;
        case ClearType::HARD_CLEAR:    clearText = "HARD CLEAR";    clearColor = {255,   0,   0, 255}; break;
        case ClearType::NORMAL_CLEAR:  clearText = "NORMAL CLEAR";  clearColor = {  0, 200, 255, 255}; break;
        case ClearType::EASY_CLEAR:    clearText = "EASY CLEAR";    clearColor = {150, 255, 100, 255}; break;
        case ClearType::ASSIST_CLEAR:  clearText = "ASSIST CLEAR";  clearColor = {180, 100, 255, 255}; break;
        case ClearType::DAN_CLEAR:     clearText = "DAN CLEAR";     clearColor = {200,   0, 100, 255}; break;
        default: break;
    }
    drawTextCached(ren, clearText, 640, 550, clearColor, true, true);

    if ((SDL_GetTicks() / 500) % 2 == 0)
        drawTextCached(ren, "PRESS ANY BUTTON TO EXIT", 640, 650, {150, 150, 150, 255}, true, true);
}





