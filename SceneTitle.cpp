#include "SceneTitle.hpp"
#include "Config.hpp"

void SceneTitle::init() {
    currentStep = TitleStep::PRESS_START;
}

TitleStep SceneTitle::update(SDL_Renderer* ren, NoteRenderer& renderer) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return currentStep;

        if (e.type == SDL_JOYBUTTONDOWN) {
            // 既存の BTN_EXIT (Start) に加え、SYS_BTN_DECIDE でも進行可能にする
            if (e.jbutton.button == Config::BTN_EXIT || e.jbutton.button == Config::SYS_BTN_DECIDE) {
                currentStep = TitleStep::FINISHED;
            }
        }
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_RETURN) {
                currentStep = TitleStep::FINISHED;
            }
        }
    }

    SDL_SetRenderDrawColor(ren, 10, 10, 30, 255);
    SDL_RenderClear(ren);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {255, 255, 0, 255};

    renderer.drawText(ren, "IIDXSWITCH TESTVER.", 640, 250, yellow, true, true);
    if ((SDL_GetTicks() / 500) % 2 == 0) {
        renderer.drawText(ren, "PRESS START BUTTON", 640, 480, white, true, true);
    }

    SDL_RenderPresent(ren);
    return currentStep;
}