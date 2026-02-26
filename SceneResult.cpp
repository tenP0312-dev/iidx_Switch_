#include "SceneResult.hpp"
#include "Config.hpp"
#include "ScoreManager.hpp" // スコア保存のために追加
#include <SDL2/SDL_image.h> // クリーンアップのために追加

void SceneResult::run(SDL_Renderer* ren, NoteRenderer& renderer, const PlayStatus& status, const BMSHeader& header) {
    // 1. スコア保存処理
    // 【修正箇所】識別精度向上のため、タイトル・難易度名・総ノーツ数を渡す新仕様に変更
    ScoreManager::saveIfBest(header.title, header.chartName, (int)header.total, status);

    // 2. 表示用に最新のベストスコアを読み込み
    // 【修正箇所】ロード側も同様に、識別用の3要素を渡すように修正
    BestScore best = ScoreManager::loadScore(header.title, header.chartName, (int)header.total);

    bool backToSelect = false;
    SDL_Event e;

    // --- 追加: 最小表示時間とチャタリング防止用の時間記録 ---
    uint32_t startTime = SDL_GetTicks();
    const uint32_t MIN_DISPLAY_TIME = 500; // 0.5秒間は入力を受け付けない
    // ---------------------------------------------------

    // 【追加】演奏シーンで使用されたリソースの解放を確実にするため、
    // 未処理のイベントを一度クリアし、OSにメモリ返却の隙を与える
    SDL_PumpEvents();

    while (!backToSelect) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return;

            // 0.5秒経過後のみ入力をチェック
            if (SDL_GetTicks() - startTime > MIN_DISPLAY_TIME) {
                if (e.type == SDL_JOYBUTTONDOWN) {
                    // SYS_BTN_BACK (または BTN_EXIT) ののみ許可
                    if (e.jbutton.button == Config::SYS_BTN_BACK) {
                        backToSelect = true;
                    }
                }
                else if (e.type == SDL_KEYDOWN) {
                    // キーボードでも ESC や BackSpace 等、システム的な戻るキーのみ許可
                    if (e.key.keysym.sym == SDLK_ESCAPE || 
                        e.key.keysym.sym == SDLK_BACKSPACE ||
                        e.key.keysym.sym == SDLK_RETURN) {
                        backToSelect = true;
                    }
                }
            }
        }

        // 背景のクリア
        SDL_SetRenderDrawColor(ren, 10, 10, 20, 255);
        SDL_RenderClear(ren);

        // ランクの計算
        std::string rank = calculateRank(status);

        // 3. レンダラーによる描画
        renderer.renderResult(ren, status, header, rank);

        // --- IIDX風 ゲージ推移グラフの描画 (ゲージオプション対応) ---
        int gx = 100, gy = 500, gw = 600, gh = 150;
        int opt = Config::GAUGE_OPTION;
        
        // 背景と枠線 (HAZARDは透過なしの黒)
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        if (opt == 6) SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); // HAZARD
        else SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        
        SDL_Rect graphBg = {gx, gy, gw, gh};
        SDL_RenderFillRect(ren, &graphBg);
        SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
        SDL_RenderDrawRect(ren, &graphBg);

        // グリッド線とクリアラインの判定
        float border = 80.0f;
        if (opt == 1) border = 60.0f; // ASSIST EASY

        for (int i = 1; i <= 4; ++i) {
            int level = i * 20;
            int lineY = gy + gh - (level * gh / 100);
            
            // クリアラインの色設定 (HARD系以上は特定のライン強調なし)
            if (opt < 3 && (int)level == (int)border) {
                SDL_SetRenderDrawColor(ren, 200, 50, 50, 255); 
            } else {
                SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
            }
            SDL_RenderDrawLine(ren, gx, lineY, gx + gw, lineY);
        }

        // ゲージ推移本体
        if (!status.gaugeHistory.empty()) {
            int historySize = (int)status.gaugeHistory.size();
            float stepX = (float)gw / (historySize > 1 ? (historySize - 1) : 1);
            
            for (int i = 0; i < historySize - 1; ++i) {
                int x1 = gx + (int)(i * stepX);
                int y1 = gy + gh - (int)(status.gaugeHistory[i] * gh / 100.0f);
                int x2 = gx + (int)((i + 1) * stepX);
                int y2 = gy + gh - (int)(status.gaugeHistory[i+1] * gh / 100.0f);

                // 色の決定ロジック
                SDL_Color lineCol, fillCol;
                if (opt == 4) { // EX-HARD
                    lineCol = {255, 255, 0, 255}; fillCol = {150, 150, 0, 100};
                } else if (opt >= 3) { // HARD / DAN / HAZARD
                    lineCol = {255, 60, 60, 255};  fillCol = {150, 30, 30, 100};
                } else { // NORMAL / EASY / ASSIST
                    if (status.gaugeHistory[i] >= border) {
                        lineCol = {255, 60, 60, 255};  fillCol = {150, 30, 30, 100};
                    } else {
                        lineCol = {60, 150, 255, 255}; fillCol = {30, 80, 150, 100};
                    }
                }

                SDL_SetRenderDrawColor(ren, lineCol.r, lineCol.g, lineCol.b, lineCol.a);
                SDL_RenderDrawLine(ren, x1, y1, x2, y2);
                
                SDL_SetRenderDrawColor(ren, fillCol.r, fillCol.g, fillCol.b, fillCol.a);
                SDL_RenderDrawLine(ren, x1, y1 + 1, x1, gy + gh - 1);
            }
        }
        // ----------------------------------------------

        // --- FAST/SLOW 内訳表示の追加 (最小限の変更) ---
        char fastText[32], slowText[32];
        snprintf(fastText, sizeof(fastText), "FAST: %d", status.fastCount);
        snprintf(slowText, sizeof(slowText), "SLOW: %d", status.slowCount);
        
        renderer.drawText(ren, fastText, 850, 580, {0, 255, 255, 255}, false, false);
        renderer.drawText(ren, slowText, 850, 610, {255, 0, 255, 255}, false, false);
        // ----------------------------------------------

        SDL_RenderPresent(ren);
        SDL_Delay(16); // 約60fpsを維持
    }

    // --- 追加: 選曲画面に戻った瞬間の誤決定防止（チャタリング対策） ---
    // 【修正】IMG_Quitを呼び出し、演奏シーンでIMG_Loadした画像キャッシュを物理的にクリアする
    IMG_Quit(); 
    SDL_Delay(200); 
    SDL_FlushEvents(SDL_JOYBUTTONDOWN, SDL_JOYBUTTONUP); // 溜まったボタン入力を完全に破棄
    // -----------------------------------------------------------
}

std::string SceneResult::calculateRank(const PlayStatus& status) {
    if (status.totalNotes <= 0) return "F";
    
    // EXスコアの計算 (P-GREAT=2, GREAT=1)
    double maxExScore = status.totalNotes * 2.0;
    double currentExScore = (status.pGreatCount * 2.0) + (status.greatCount * 1.0);
    double ratio = currentExScore / maxExScore;

    // BMS標準のランク閾値 (九分率)
    if (ratio >= 8.0/9.0) return "AAA";
    if (ratio >= 7.0/9.0) return "AA";
    if (ratio >= 6.0/9.0) return "A";
    if (ratio >= 5.0/9.0) return "B";
    if (ratio >= 4.0/9.0) return "C";
    if (ratio >= 3.0/9.0) return "D";
    if (ratio >= 2.0/9.0) return "E";
    return "F";
}