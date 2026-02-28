#ifndef SCENE_GAMEOVER_HPP
#define SCENE_GAMEOVER_HPP

#include <SDL2/SDL.h>
#include "NoteRenderer.hpp"

class SceneGameOver {
public:
    void init() { startTime = SDL_GetTicks(); }
    // 演出終了ならtrueを返す
    bool update(SDL_Renderer* ren, NoteRenderer& renderer) {
        Uint32 elapsed = SDL_GetTicks() - startTime;
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        SDL_Color white = {255, 255, 255, 255};
        // ★修正: drawText → drawTextCached に変更。
        //        drawText は「毎フレーム内容が変化するテキスト専用」とヘッダに明記されているが、
        //        "GAME OVER" は固定文字列。2秒間毎フレーム TTF_RenderUTF8_Blended +
        //        SDL_CreateTextureFromSurface + SDL_DestroyTexture が走っていた。
        //        drawTextCached は初回のみテクスチャを生成し以降はキャッシュを再利用する。
        renderer.drawTextCached(ren, "GAME OVER", 640, 360, white, true, true);

        SDL_RenderPresent(ren);
        return (elapsed > 2000); // 2秒演出
    }
private:
    Uint32 startTime;
};

#endif





