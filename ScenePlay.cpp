#include "ScenePlay.hpp"
#include "PlayEngine.hpp"
#include "Config.hpp"
#include "SceneResult.hpp"
#include "BgaManager.hpp"
#include <cmath>
#include <algorithm>
#include <SDL2/SDL_image.h> 

#ifdef __SWITCH__
#include <switch.h>
#endif

// --- 既存の補助関数群 (100%継承) ---
int ScenePlay::getLaneFromJoystickButton(int btn) {
    if (btn == Config::BTN_LANE1) return 1;
    if (btn == Config::BTN_LANE2) return 2;
    if (btn == Config::BTN_LANE3) return 3;
    if (btn == Config::BTN_LANE4) return 4;
    if (btn == Config::BTN_LANE5) return 5;
    if (btn == Config::BTN_LANE6) return 6;
    if (btn == Config::BTN_LANE7) return 7;
    if (btn == Config::BTN_LANE8_A || btn == Config::BTN_LANE8_B) return 8;
    return -1;
}

bool ScenePlay::isAutoLane(int lane) {
    if (Config::ASSIST_OPTION == 7) return true;
    bool autoScr = (Config::ASSIST_OPTION == 1 || Config::ASSIST_OPTION == 4 || 
                    Config::ASSIST_OPTION == 5 || Config::ASSIST_OPTION == 6);
    bool auto5k = (Config::ASSIST_OPTION == 3 || Config::ASSIST_OPTION == 5 || 
                    Config::ASSIST_OPTION == 6);
    if (lane == 8 && autoScr) return true;
    if ((lane == 6 || lane == 7) && auto5k) return true;
    return false;
}

void ScenePlay::updateAssist(double cur_ms, PlayEngine& engine, SoundManager& snd) {
    uint32_t now = SDL_GetTicks();
    for (auto& n : engine.getNotes()) {
        if (n.played || n.isBGM) continue;
        if (n.target_ms > cur_ms + 100) break;
        if (isAutoLane(n.lane)) {
            if (!n.isBeingPressed && cur_ms >= n.target_ms) {
                engine.processHit(n.lane, n.target_ms, now, snd);
                
                bool found = false;
                for (auto& eff : effects) {
                    if (eff.lane == n.lane) {
                        eff.startTime = now;
                        found = true;
                        break;
                    }
                }
                if (!found) effects.push_back({n.lane, now});
                
                bombAnims.push_back({n.lane, now, 2});
            }
            if (n.isLN && n.isBeingPressed && cur_ms >= n.target_ms + n.duration_ms) {
                engine.processRelease(n.lane, n.target_ms + n.duration_ms, now);
            }
        }
    }
}

// --- メインロジック ---
bool ScenePlay::run(SDL_Renderer* ren, SoundManager& snd, NoteRenderer& renderer, const std::string& bmsonPath) {
    {
        BgaManager tempBga;
        tempBga.cleanup(); 
    }
    snd.clear(); 
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);
    SDL_Delay(200); 

    IMG_Quit();
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    IMG_Init(imgFlags);
    SDL_Delay(50);

    int lastParsePercent = -1;
    BMSData data = BmsonLoader::load(bmsonPath, [&](float progress) {
        int curPercent = (int)(progress * 100);
        if (curPercent != lastParsePercent) {
            renderer.renderLoading(ren, curPercent, 100, "Parsing Bmson...");
            SDL_RenderPresent(ren);
            lastParsePercent = curPercent;
        }
        SDL_Event e; while(SDL_PollEvent(&e));
    });

    if (data.sound_channels.empty()) return true;

    currentHeader = data.header;
    Config::HIGH_SPEED = (double)Config::HS_BASE / (std::max(1, Config::GREEN_NUMBER) * data.header.bpm);

    PlayEngine engine;
    engine.init(data);
    
    BgaManager bga;
    bga.init(data.bga_images.size());
    bga.setEvents(data.bga_events);      
    bga.setLayerEvents(data.layer_events); 
    bga.setPoorEvents(data.poor_events);   

    effects.clear();
    bombAnims.clear(); 
    for(int i=0; i<9; ++i) lanePressed[i] = false;

    isAssistUsed = (Config::ASSIST_OPTION > 0);
    startButtonPressed = false;
    effectButtonPressed = false;
    scratchUpActive = false;
    scratchDownActive = false;
    lastStartPressTime = 0;
    if (Config::SUDDEN_PLUS > 0) backupSudden = Config::SUDDEN_PLUS;

    std::string bmsonDir = "";
    size_t lastSlash = bmsonPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        bmsonDir = bmsonPath.substr(0, lastSlash + 1);
    } else {
        bmsonDir = Config::ROOT_PATH;
    }

    bga.setBgaDirectory(bmsonDir);

    if (!data.header.bga_video.empty()) {
        bga.loadBgaFile(bmsonDir + data.header.bga_video, ren);
    }

    for (auto const& [id, filename] : data.bga_images) {
        bga.registerPath(id, filename);
    }

    SDL_Delay(50);

    std::string bmsonBaseName = "";
    size_t lastDot = bmsonPath.find_last_of(".");
    if (lastDot != std::string::npos && lastDot > lastSlash) {
        bmsonBaseName = bmsonPath.substr(lastSlash + 1, lastDot - lastSlash - 1);
    } else {
        bmsonBaseName = data.header.title; 
    }

    renderer.renderLoading(ren, 0, (int)data.sound_channels.size(), "Indexing BoxWav Files...");
    SDL_RenderPresent(ren);
    snd.preloadBoxIndex(bmsonDir, bmsonBaseName);

    int lastPercent = -1;
    for (int i = 0; i < (int)data.sound_channels.size(); ++i) {
        int curPercent = (i * 100) / (int)data.sound_channels.size();
        if (curPercent != lastPercent) {
            renderer.renderLoading(ren, i + 1, data.sound_channels.size(), "Audio Loading: " + data.sound_channels[i].name);
            uint64_t curMem = snd.getCurrentMemory();
            uint64_t maxMem = snd.getMaxMemory();
            double curMB = (double)curMem / (1024.0 * 1024.0);
            double maxMB = (double)maxMem / (1024.0 * 1024.0);
            char memBuf[128];
            snprintf(memBuf, sizeof(memBuf), "WAV Memory: %.1f / %.1f MB", curMB, maxMB);
            renderer.drawText(ren, memBuf, 640, 580, {200, 200, 200, 255}, false, true);
            if (curMem >= maxMem - (1024 * 10)) { 
                renderer.drawText(ren, "WARNING: MEMORY LIMIT REACHED (SKIPPING)", 640, 620, {255, 50, 50, 255}, false, true);
            }
            SDL_RenderPresent(ren);
            lastPercent = curPercent;
        }
        snd.loadSingleSound(data.sound_channels[i].name, bmsonDir, bmsonBaseName);
        if (i % 100 == 0) {
            SDL_Event e; while(SDL_PollEvent(&e));
            SDL_Delay(5);
        }
    }

    SDL_Delay(100);

    double max_target_ms = 0;
    for (const auto& n : engine.getNotes()) {
        if (!n.isBGM) max_target_ms = std::max(max_target_ms, n.target_ms);
    }

    uint32_t readyStartTime = SDL_GetTicks();
    const uint32_t READY_DURATION = 5000; 
    while (SDL_GetTicks() - readyStartTime < READY_DURATION) {
        uint32_t now = SDL_GetTicks();
        if (!processInput(-2000.0, now, snd, engine)) return false;
        bga.preLoad(0, ren);
        renderScene(ren, renderer, engine, bga, -2000.0, 0, 0, currentHeader, now, 0.0);
        int totalWidth = (7 * Config::LANE_WIDTH) + Config::SCRATCH_WIDTH + 10;
        int baseX = (Config::PLAY_SIDE == 1) ? 50 : (1280 - totalWidth - 50);
        int laneCenterX = baseX + totalWidth / 2;
        renderer.drawText(ren, "Please wait 5 seconds", laneCenterX, 450, {255, 255, 0, 255}, false, true);
        SDL_RenderPresent(ren);
#ifdef __SWITCH__
        if (!appletMainLoop()) return false;
#endif
    }

    bool waiting = true;
    while (waiting) {
        uint32_t now = SDL_GetTicks();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) return false;
            if (ev.type == SDL_JOYBUTTONDOWN) {
                if (ev.jbutton.button == Config::SYS_BTN_DECIDE) {
                    waiting = false;
                    break;
                }
            }
        }
        if (!processInput(-2000.0, now, snd, engine)) return false;
        renderScene(ren, renderer, engine, bga, -2000.0, 0, 0, currentHeader, now, 0.0);
        int totalWidth = (7 * Config::LANE_WIDTH) + Config::SCRATCH_WIDTH + 10;
        int baseX = (Config::PLAY_SIDE == 1) ? 50 : (1280 - totalWidth - 50);
        int laneCenterX = baseX + totalWidth / 2;
        renderer.drawText(ren, "PRESS DECIDE BUTTON TO START", laneCenterX, 450, {255, 255, 255, 255}, false, true);
        SDL_RenderPresent(ren);
#ifdef __SWITCH__
        if (!appletMainLoop()) return false;
#endif
    }

    uint32_t start_ticks = SDL_GetTicks() + 2000;
    uint32_t lastFpsTime = SDL_GetTicks();
    int frameCount = 0, fps = 0;
    bool playing = true;
    bool isAborted = false;
    bool fcEffectTriggered = false; 

    SDL_Texture* gradTex = nullptr;
    const int TEX_H = 512;
    {
        Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000; gmask = 0x00ff0000; bmask = 0x0000ff00; amask = 0x000000ff;
#else
        rmask = 0x000000ff; gmask = 0x0000ff00; bmask = 0x00ff0000; amask = 0xff000000;
#endif
        SDL_Surface* surf = SDL_CreateRGBSurface(0, 1, TEX_H, 32, rmask, gmask, bmask, amask);
        if (surf) {
            Uint32* pixels = (Uint32*)surf->pixels;
            for (int y = 0; y < TEX_H; y++) {
                float dist = std::abs((float)y - (TEX_H / 2.0f)) / (TEX_H / 2.0f);
                Uint8 alpha = (Uint8)((1.0f - dist) * 255);
                pixels[y] = SDL_MapRGBA(surf->format, 255, 255, 255, alpha);
            }
            gradTex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_FreeSurface(surf);
        }
    }

    while (playing) {
        uint32_t now = SDL_GetTicks();
        double cur_ms = (double)((int64_t)now - (int64_t)start_ticks);
        if (!processInput(cur_ms, now, snd, engine)) {
            if (engine.getStatus().isFailed) playing = false;
            else { isAborted = true; playing = false; break; }
        }
        updateAssist(cur_ms, engine, snd);
        engine.update(cur_ms + 10.0, now, snd);
        status = engine.getStatus(); 
        if (status.isFailed) playing = false;
        double progress = 0.0;
        if (max_target_ms > 0) progress = std::clamp(cur_ms / max_target_ms, 0.0, 1.0);
        int64_t cur_y = engine.getYFromMs(cur_ms);
        auto& judge = engine.getCurrentJudge();
        if (judge.active && (judge.text == "MISS" || judge.text == "POOR")) bga.setMissTrigger(true);
        else bga.setMissTrigger(false);

        renderScene(ren, renderer, engine, bga, cur_ms, cur_y, fps, currentHeader, now, progress);

        // --- フルコンボ判定と演出 ---
        if (!fcEffectTriggered && status.remainingNotes <= 0) {
            // 修正：判定条件を「ミスなし かつ 1枚以上のノーツを完走」に変更
            bool isFC = (status.poorCount == 0 && status.badCount == 0 && status.totalNotes > 0);
            if (isFC) {
                // 重要：status.clearTypeをFULL_COMBOに更新し、ScoreManagerが上位記録として認識できるようにする
                status.clearType = ClearType::FULL_COMBO;

                fcEffectTriggered = true; 
                uint32_t fcStart = SDL_GetTicks();
                while (SDL_GetTicks() - fcStart < 2500) {
                    uint32_t nowFC = SDL_GetTicks();
                    float p = std::min(1.0f, (float)(nowFC - fcStart) / 1000.0f);
                    renderScene(ren, renderer, engine, bga, cur_ms, cur_y, fps, currentHeader, nowFC, 1.0);
                    int totalWidth = (7 * Config::LANE_WIDTH) + Config::SCRATCH_WIDTH + 10;
                    int baseX = (Config::PLAY_SIDE == 1) ? 50 : (1280 - totalWidth - 50);
                    int laneCenterX = baseX + totalWidth / 2;
                    if (gradTex) {
                        int lineY = Config::JUDGMENT_LINE_Y;
                        int uvOffset = (int)(nowFC * 1) % TEX_H; 
                        SDL_Rect srcRect = { 0, uvOffset, 1, TEX_H / 2 };
                        SDL_Rect dstRect = { baseX, 0, totalWidth, lineY }; 
                        SDL_SetTextureBlendMode(gradTex, SDL_BLENDMODE_BLEND);
                        SDL_SetTextureColorMod(gradTex, 0, 255, 255);
                        SDL_SetTextureAlphaMod(gradTex, (Uint8)((1.0f - p) * 200)); 
                        SDL_RenderCopy(ren, gradTex, &srcRect, &dstRect);
                        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(ren, 0, 255, 255, (Uint8)((1.0f - p) * 255));
                        SDL_Rect brightLine = { baseX, lineY - 3, totalWidth, 6 };
                        SDL_RenderFillRect(ren, &brightLine);
                        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                    }
                    SDL_Color fcColor;
                    uint32_t t = nowFC / 80;
                    if (t % 3 == 0)      fcColor = {0, 255, 255, 255}; 
                    else if (t % 3 == 1) fcColor = {0, 200, 255, 255}; 
                    else                  fcColor = {255, 255, 255, 255};
                    renderer.drawText(ren, "FULL COMBO", laneCenterX, 200, fcColor, true, true);
                    SDL_RenderPresent(ren);
                    SDL_Delay(1);
#ifdef __SWITCH__
                    if (!appletMainLoop()) break;
#endif
                }
                playing = false; break;           
            }
        }
        if (cur_ms > status.maxTargetMs + 1500.0) playing = false;
        frameCount++;
        if (now - lastFpsTime >= 1000) { fps = frameCount; frameCount = 0; lastFpsTime = now; }
#ifdef __SWITCH__
        if (!appletMainLoop()) { isAborted = true; playing = false; break; }
#endif
    }

    Config::save();

    if (gradTex) SDL_DestroyTexture(gradTex);
    snd.clear();
    bga.cleanup(); 
    if (isAborted) return false; 
    return true;
}

// --- 入力処理 ---
bool ScenePlay::processInput(double cur_ms, uint32_t now, SoundManager& snd, PlayEngine& engine) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) return false;

        if (ev.type == SDL_JOYBUTTONDOWN || ev.type == SDL_JOYBUTTONUP) {
            bool isDown = (ev.type == SDL_JOYBUTTONDOWN);
            int btn = ev.jbutton.button;
            int lane = getLaneFromJoystickButton(btn);

            if (lane != -1) lanePressed[lane] = isDown;

            if (btn == Config::BTN_EXIT) {
                if (isDown) {
                    if (now - lastStartPressTime < 500) {
                        if (Config::SUDDEN_PLUS > 0) {
                            backupSudden = Config::SUDDEN_PLUS;
                            Config::SUDDEN_PLUS = 0;
                        } else {
                            Config::SUDDEN_PLUS = backupSudden;
                        }
                        lastStartPressTime = 0; 
                    } else {
                        lastStartPressTime = now;
                    }
                }
                startButtonPressed = isDown;
            }
            if (btn == Config::BTN_EFFECT) effectButtonPressed = isDown;
            if (btn == Config::BTN_LANE8_A) scratchUpActive = isDown;
            if (btn == Config::BTN_LANE8_B) scratchDownActive = isDown;

            if (startButtonPressed && effectButtonPressed) { engine.forceFail(); return false; }

            if (isDown && startButtonPressed && lane != -1 && lane <= 7) {
                double currentBPM = engine.getBpmFromMs(cur_ms);
                int effectiveGN = (int)(Config::HS_BASE / (std::max(0.01, Config::HIGH_SPEED) * currentBPM));
                if (lane == 1)      effectiveGN += 10;
                else if (lane == 2) effectiveGN -= 10;
                else if (lane == 3) effectiveGN += 25;
                else if (lane == 4) effectiveGN -= 25;
                else if (lane == 5) effectiveGN += 50;
                else if (lane == 6) effectiveGN -= 50;
                else if (lane == 7) effectiveGN = 1200;
                Config::GREEN_NUMBER = std::clamp(effectiveGN, 1, 9999);
                Config::HIGH_SPEED = (double)Config::HS_BASE / (Config::GREEN_NUMBER * currentBPM);
                continue; 
            }

            if (lane != -1 && !isAutoLane(lane)) {
                if (isDown) {
                    if (!engine.getStatus().isFailed && cur_ms >= -500.0) {
                        int resultJudge = engine.processHit(lane, cur_ms, now, snd);
                        
                        bool found = false;
                        for (auto& eff : effects) {
                            if (eff.lane == lane) {
                                eff.startTime = now; 
                                found = true;
                                break;
                            }
                        }
                        if (!found) effects.push_back({lane, now});

                        if (resultJudge >= 2) {
                            int bombType = (resultJudge == 3) ? 1 : 2;
                            bombAnims.push_back({lane, now, bombType});
                        }
                    }
                } else {
                    if (!engine.getStatus().isFailed && cur_ms >= -500.0) {
                        engine.processRelease(lane, cur_ms, now);
                    }
                }
            }
        }
    }
    if (startButtonPressed && (scratchUpActive || scratchDownActive)) {
        int delta = scratchUpActive ? -2 : 2; 
        Config::SUDDEN_PLUS = std::clamp(Config::SUDDEN_PLUS + delta, 0, 1000);
        if (Config::SUDDEN_PLUS > 0) backupSudden = Config::SUDDEN_PLUS;
    }
    return true;
}

void ScenePlay::renderScene(SDL_Renderer* ren, NoteRenderer& renderer, PlayEngine& engine, BgaManager& bga, double cur_ms, int64_t cur_y, int fps, const BMSHeader& header, uint32_t now, double progress) {
    SDL_SetRenderDrawColor(ren, 10, 10, 15, 255);
    SDL_RenderClear(ren);
    int bgaX = (Config::PLAY_SIDE == 1) ? 600 : 40; 
    int bgaY = 40; 
    bga.render(cur_y, ren, bgaX, bgaY, cur_ms);
    renderer.renderLanes(ren, progress);
    double currentBpm = engine.getBpmFromMs(cur_ms);
    double visual_speed = (Config::HIGH_SPEED * currentBpm) / 475.0; 
    double max_visible_ms = (double)Config::VISIBLE_PX / std::max(0.01, visual_speed) + 200.0;

    for (const auto& bl : engine.getBeatLines()) {
        double y_per_ms = (currentBpm * header.resolution) / 60000.0;
        double y_diff_ms = (double)(bl.y - cur_y) / y_per_ms;
        if (y_diff_ms > -500.0 && y_diff_ms < max_visible_ms) renderer.renderBeatLine(ren, y_diff_ms, visual_speed);
    }

    // --- エフェクト描画 ---
    for (auto it = effects.begin(); it != effects.end(); ) {
        if (it->lane >= 1 && it->lane <= 7 && lanePressed[it->lane]) {
            it->startTime = now; 
        }
        float duration = (it->lane == 8) ? 200.0f : 100.0f;
        float p = (float)(now - it->startTime) / duration; 
        if (p >= 1.0f) it = effects.erase(it);
        else { renderer.renderHitEffect(ren, it->lane, p); ++it; }
    }

    // --- ボム描画 ---
    for (auto it = bombAnims.begin(); it != bombAnims.end(); ) {
        float p = (float)(now - it->startTime) / 300.0f;
        if (p >= 1.0f) it = bombAnims.erase(it);
        else {
            if (it->judgeType == 1 || it->judgeType == 2) {
                renderer.renderBomb(ren, it->lane, (int)(p * 10));
            }
            ++it;
        }
    }

    for (const auto& n : engine.getNotes()) {
        if ((!n.played || n.isBeingPressed) && !n.isBGM) {
            double y_per_ms = (currentBpm * header.resolution) / 60000.0;
            double y_diff_ms = (double)(n.y - cur_y) / y_per_ms;
            double end_y_diff_ms = y_diff_ms;
            if (n.isLN) end_y_diff_ms += n.duration_ms; 
            if (end_y_diff_ms > -500.0 && y_diff_ms < max_visible_ms) {
                PlayableNote tempNote = n;
                tempNote.target_ms = cur_ms + y_diff_ms;
                renderer.renderNote(ren, tempNote, cur_ms, visual_speed, isAutoLane(n.lane));
            }
        }
    }

    int totalWidth = (7 * Config::LANE_WIDTH) + Config::SCRATCH_WIDTH + 10;
    int baseX = (Config::PLAY_SIDE == 1) ? 50 : (1280 - totalWidth - 50);
    int laneCenterX = baseX + totalWidth / 2;

    auto& judge = engine.getCurrentJudge();
    if (judge.active) {
        float p_raw = (float)(now - judge.startTime) / 500.0f;
        if (p_raw >= 1.0f) judge.active = false;
        else {
            SDL_Color drawColor = judge.color; 
            std::string drawText = judge.text; // 元の "P-GREAT" を維持
            bool shouldDraw = true; 
            uint32_t frameUnit = now / 32;

            if (judge.text == "P-GREAT") {
                // P-GREATの時は点滅させず、Renderer側のアニメーションに任せる
                // drawText = "GREAT";  <-- これを削除またはコメントアウト
            } else { 
                // GREAT以下の場合は点滅（1フレームおきに非表示）
                if (frameUnit % 2 == 0) shouldDraw = false; 
            }
            if (shouldDraw) {
                renderer.renderJudgment(ren, drawText, 0.0f, drawColor, engine.getStatus().combo);
                if (Config::SHOW_FAST_SLOW && judge.text != "P-GREAT" && judge.text != "POOR" && judge.text != "MISS") {
                    if (judge.isFast) renderer.drawText(ren, "FAST", laneCenterX, 365, {0, 255, 255, 255}, false, true);
                    else if (judge.isSlow) renderer.drawText(ren, "SLOW", laneCenterX, 365, {255, 0, 255, 255}, false, true);
                }
            }
        }
    }

    renderer.renderCombo(ren, engine.getStatus().combo);
    renderer.renderGauge(ren, engine.getStatus().gauge, Config::GAUGE_OPTION, engine.getStatus().isFailed);
    renderer.renderUI(ren, header, fps, currentBpm, engine.getStatus().exScore);

    if (startButtonPressed) {
        double hs = std::max(0.01, Config::HIGH_SPEED);
        int effectiveHeight = Config::JUDGMENT_LINE_Y - Config::SUDDEN_PLUS;
        auto calcSyncGN = [&](double bpm) {
            int baseGN = (int)(Config::HS_BASE / (hs * bpm));
            return (int)(baseGN * (double)effectiveHeight / Config::JUDGMENT_LINE_Y);
        };
        int curGN = calcSyncGN(currentBpm);
        char gearText[256];
        if (header.min_bpm > 0 && header.max_bpm > 0 && header.min_bpm != header.max_bpm) {
            int maxGN = calcSyncGN(header.min_bpm);
            int minGN = calcSyncGN(header.max_bpm);
            snprintf(gearText, sizeof(gearText), "GN: %d - %d - %d | SUD+:%d LIFT:%d", maxGN, curGN, minGN, Config::SUDDEN_PLUS, Config::LIFT);
        } else {
            snprintf(gearText, sizeof(gearText), "GN: %d | SUD+:%d LIFT:%d", curGN, Config::SUDDEN_PLUS, Config::LIFT);
        }
        renderer.drawText(ren, gearText, laneCenterX, 20, {0, 255, 0, 255}, false, true);
    }
    SDL_RenderPresent(ren);
}