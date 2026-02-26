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
    const auto& notes = engine.getNotes();
    // 描画開始位置から探索を開始することで計算量を削減
    for (size_t i = drawStartIndex; i < notes.size(); ++i) {
        const auto& n = notes[i];
        if (n.played || n.isBGM) continue;
        if (n.target_ms > cur_ms + 100) break; // 早期終了
        
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
    // 1. 前の曲の残骸を完全に消し去る (断片化対策の第一歩)
    {
        BgaManager tempBga;
        tempBga.cleanup(); 
    }
    snd.clear(); 
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);
    SDL_Delay(200); 

    // 2. BMSONのパース (この内部でJSONがパースされ、そして即座に破棄される)
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

    // 3. パース終了～音声ロード開始の「隙間」を作る
    // ここでJSONの巨大なメモリが解放され、ヒープに空きができる
    SDL_Delay(100); 

    currentHeader = data.header;
    Config::HIGH_SPEED = (double)Config::HS_BASE / (std::max(1, Config::GREEN_NUMBER) * data.header.bpm);

    PlayEngine engine;
    engine.init(data);
    drawStartIndex = 0;
    
    // BGA初期化
    BgaManager bga;
    bga.init(data.bga_images.size());
    bga.setEvents(data.bga_events);      
    bga.setLayerEvents(data.layer_events); 
    bga.setPoorEvents(data.poor_events);   

    effects.clear();
    effects.reserve(64); 
    bombAnims.clear(); 
    bombAnims.reserve(64); 
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
        std::string videoFile = data.header.bga_video;
        std::string fullVideoPath = bmsonDir + videoFile;
        bga.loadBgaFile(fullVideoPath, ren);
    }

    for (auto const& [id, filename] : data.bga_images) {
        bga.registerPath(id, filename);
    }

    // 4. 音声インデックス作成とバルクロード
    // JSONが消えて「きれいになったヒープ」に対して大きな音声を確保しにいく
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

    std::vector<std::string> soundList;
    soundList.reserve(data.sound_channels.size());
    for (const auto& ch : data.sound_channels) {
        soundList.push_back(ch.name);
    }

    // 指摘のあった「二重消費」はSoundManager側で修正済みのため、安心して呼べる
    snd.loadSoundsInBulk(soundList, bmsonDir, bmsonBaseName, [&](int processedCount, const std::string& currentName) {
        int curPercent = (processedCount * 100) / (int)data.sound_channels.size();
        static int lastPercent = -1;

        if (curPercent != lastPercent) {
            renderer.renderLoading(ren, processedCount, data.sound_channels.size(), "Audio Loading: " + currentName);
            
            uint64_t curMem = snd.getCurrentMemory();
            uint64_t maxMem = snd.getMaxMemory();
            double curMB = (double)curMem / (1024.0 * 1024.0);
            double maxMB = (double)maxMem / (1024.0 * 1024.0);
            char memBuf[128];
            snprintf(memBuf, sizeof(memBuf), "WAV Memory: %.1f / %.1f MB", curMB, maxMB);
            renderer.drawText(ren, memBuf, 640, 580, {200, 200, 200, 255}, false, true);
            
            if (curMem >= maxMem - (1024 * 1024 * 5)) { // 警告しきい値を5MB程度に調整
                renderer.drawText(ren, "WARNING: MEMORY LIMIT REACHED (SKIPPING)", 640, 620, {255, 50, 50, 255}, false, true);
            }
            
            SDL_RenderPresent(ren);
            lastPercent = curPercent;
        }

        if (processedCount % 100 == 0) {
            SDL_Event e; while(SDL_PollEvent(&e));
        }
    });

    SDL_Delay(100);

    double videoOffsetMs = 0.0;
    if (data.header.bga_offset != 0) {
        double currentBpm = data.header.bpm;
        int64_t currentY = 0;
        double currentMs = 0.0;
        std::vector<BPMEvent> sortedBpm = data.bpm_events;
        std::sort(sortedBpm.begin(), sortedBpm.end(), [](const BPMEvent& a, const BPMEvent& b){ return a.y < b.y; });
        for (const auto& bpmEv : sortedBpm) {
            if (bpmEv.y >= data.header.bga_offset) break;
            int64_t distY = bpmEv.y - currentY;
            currentMs += (double)distY * 60000.0 / (currentBpm * data.header.resolution);
            currentY = bpmEv.y;
            currentBpm = bpmEv.bpm;
        }
        if (currentY < data.header.bga_offset) {
            int64_t distY = data.header.bga_offset - currentY;
            currentMs += (double)distY * 60000.0 / (currentBpm * data.header.resolution);
        }
        videoOffsetMs = currentMs;
    }

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

        bga.syncTime(cur_ms - videoOffsetMs);

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

        if (!fcEffectTriggered && status.remainingNotes <= 0) {
            bool isFC = (status.poorCount == 0 && status.badCount == 0 && status.totalNotes > 0);
            if (isFC) {
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
    renderer.renderBackground(ren);
    int bgaX = (Config::PLAY_SIDE == 1) ? 600 : 40; 
    int bgaY = 40; 
    double currentBpm = engine.getBpmFromMs(cur_ms);
    renderer.renderUI(ren, header, fps, currentBpm, engine.getStatus().exScore);
    bga.render(cur_y, ren, bgaX, bgaY, cur_ms);
    renderer.renderLanes(ren, progress);
    
    double visual_speed = (Config::HIGH_SPEED * currentBpm) / 475.0; 
    double max_visible_ms = (double)Config::VISIBLE_PX / std::max(0.01, visual_speed) + 200.0;
    double y_per_ms = (currentBpm * header.resolution) / 60000.0;

    // 小節線の描画最適化（本来はここもインデックス管理すべきだが、数が少ないため現状維持）
    for (const auto& bl : engine.getBeatLines()) {
        double y_diff_ms = (double)(bl.y - cur_y) / y_per_ms;
        if (y_diff_ms > -500.0 && y_diff_ms < max_visible_ms) renderer.renderBeatLine(ren, y_diff_ms, visual_speed);
    }

    // エフェクト・ボム描画 (既存ロジック100%継承)
    effects.erase(std::remove_if(effects.begin(), effects.end(), [&](auto& eff) {
        if (eff.lane >= 1 && eff.lane <= 7 && lanePressed[eff.lane]) eff.startTime = now; 
        float duration = (eff.lane == 8) ? 200.0f : 100.0f;
        float p = (float)(now - eff.startTime) / duration; 
        if (p >= 1.0f) return true;
        renderer.renderHitEffect(ren, eff.lane, p);
        return false;
    }), effects.end());

    bombAnims.erase(std::remove_if(bombAnims.begin(), bombAnims.end(), [&](auto& ba) {
        float p = (float)(now - ba.startTime) / 300.0f;
        if (p >= 1.0f) return true;
        if (ba.judgeType == 1 || ba.judgeType == 2) renderer.renderBomb(ren, ba.lane, (int)(p * 10));
        return false;
    }), bombAnims.end());

    // --- 【劇的改善】ノーツ描画のスライディング・ウィンドウ ---
    const auto& allNotes = engine.getNotes();
    
    // 1. 画面下端よりも遥か後ろに消えたノーツまで開始位置を進める
    while (drawStartIndex < allNotes.size() && allNotes[drawStartIndex].target_ms < cur_ms - 1000.0) {
        drawStartIndex++;
    }

    // 2. 開始位置から描画ループ
    for (size_t i = drawStartIndex; i < allNotes.size(); ++i) {
        const auto& n = allNotes[i]; // ★重要：コピーせず参照で受ける

        // 3. 画面上端（可視範囲）を超えたら即終了 (O(N) -> O(画面内ノーツ数))
        double y_diff_ms = (double)(n.y - cur_y) / y_per_ms;
        if (y_diff_ms > max_visible_ms) break; 

        if ((!n.played || n.isBeingPressed) && !n.isBGM) {
            double end_y_diff_ms = y_diff_ms;
            if (n.isLN) end_y_diff_ms += n.duration_ms; 
            
            if (end_y_diff_ms > -500.0) {
                // PlayableNote の一時オブジェクト作成を回避し、const参照のまま渡す
                // (NoteRenderer::renderNote も const PlayableNote& 受けに修正されている前提)
                renderer.renderNote(ren, n, cur_ms, visual_speed, isAutoLane(n.lane));
            }
        }
    }

    // --- (以下、コンボ・判定・ゲージ等の描画ロジック 100%継承) ---
    int totalWidth = (7 * Config::LANE_WIDTH) + Config::SCRATCH_WIDTH + 10;
    int baseX = (Config::PLAY_SIDE == 1) ? 50 : (1280 - totalWidth - 50);
    int laneCenterX = baseX + totalWidth / 2;

    auto& judge = engine.getCurrentJudge();
    if (judge.active) {
        float p_raw = (float)(now - judge.startTime) / 500.0f;
        if (p_raw >= 1.0f) judge.active = false;
        else {
            if (judge.text == "P-GREAT" || (now / 32) % 2 != 0) {
                renderer.renderJudgment(ren, judge.text, 0.0f, judge.color, engine.getStatus().combo);
            }
        }
    }

    renderer.renderCombo(ren, engine.getStatus().combo);
    renderer.renderGauge(ren, engine.getStatus().gauge, Config::GAUGE_OPTION, engine.getStatus().isFailed);

    if (startButtonPressed) {
        double hs = std::max(0.01, Config::HIGH_SPEED);
        int effectiveHeight = Config::JUDGMENT_LINE_Y - Config::SUDDEN_PLUS;
        auto calcSyncGN = [&](double bpm) {
            return (int)((Config::HS_BASE / (hs * bpm)) * (double)effectiveHeight / Config::JUDGMENT_LINE_Y);
        };
        char gearText[256];
        snprintf(gearText, sizeof(gearText), "GN: %d | SUD+:%d LIFT:%d", calcSyncGN(currentBpm), Config::SUDDEN_PLUS, Config::LIFT);
        renderer.drawText(ren, gearText, laneCenterX, 20, {0, 255, 0, 255}, false, true);
    }
    SDL_RenderPresent(ren);
}
