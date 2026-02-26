#include "SceneDecision.hpp"
#include "Config.hpp"

bool SceneDecision::run(SDL_Renderer* ren, NoteRenderer& renderer, const BMSHeader& header) {
    // 既存の描画ロジックを100%継承
    // 描画処理
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    // NoteRendererに実装した情報を描画
    renderer.renderDecisionInfo(ren, header);

    // 画面に反映
    SDL_RenderPresent(ren);

    // 指定された要件に基づき、ボタン入力を待たず3秒間（3000ms）表示
    SDL_Delay(3000);

    // プレイ画面へ遷移するため常にtrueを返す
    return true;
}