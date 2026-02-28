#include "SceneDecision.hpp"
#include "Config.hpp"
#include <SDL2/SDL.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

bool SceneDecision::run(SDL_Renderer* ren, NoteRenderer& renderer, const BMSHeader& header) {
    uint32_t startTime = SDL_GetTicks();
    const uint32_t WAIT_MS = 3000;

    // メインループを回しながら待機
    while (SDL_GetTicks() - startTime < WAIT_MS) {
        uint32_t now = SDL_GetTicks();
        
        // --- 1. OSおよび入力イベントの処理 ---
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) return false;
            
            // 待機中であっても決定ボタンが押されたら即開始するなどの
            // ユーザー体験向上を入れる場合はここに記述（今回は3秒待機を維持）
        }

        #ifdef __SWITCH__
        // Switch特有のシステム要求（スリープ、ホームボタン等）をチェック
        if (!appletMainLoop()) return false;
        #endif

        // --- 2. 描画処理 (既存ロジック100%継承) ---
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        // 既存の情報を描画
        renderer.renderDecisionInfo(ren, header);

        // 必要であれば「PRESS ANY BUTTON TO SKIP」などの点滅表示もここで可能
        
        // 画面に反映
        SDL_RenderPresent(ren);

        // --- 3. CPU負荷の調整 ---
        // 毎フレーム全力で回すとバッテリーを食うため、1ms程度休ませる
        SDL_Delay(1);
    }

    return true;
}



