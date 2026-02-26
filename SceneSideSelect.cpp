#include "SceneSideSelect.hpp"
#include "Config.hpp"
#include <string>

void SceneSideSelect::init() {
    // Configから現在のサイド設定を読み込む (1なら0(1P)、2なら1(2P))
    selectedSide = (Config::PLAY_SIDE == 2) ? 1 : 0;
}

SideSelectStep SceneSideSelect::update(SDL_Renderer* ren, NoteRenderer& renderer) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return SideSelectStep::SELECTING;

        if (e.type == SDL_JOYBUTTONDOWN) {
            int btn = e.jbutton.button;
            // プレイ用ボタン(BTN_EXIT等)を廃止し、システム専用ボタンのみで判定
            if (btn == Config::SYS_BTN_DECIDE) {
                Config::PLAY_SIDE = (selectedSide == 0) ? 1 : 2;
                return SideSelectStep::FINISHED;
            }
            if (btn == Config::SYS_BTN_LEFT || btn == Config::SYS_BTN_RIGHT ||
                btn == Config::SYS_BTN_UP || btn == Config::SYS_BTN_DOWN) {
                selectedSide = 1 - selectedSide;
            }
        }
        
        if (e.type == SDL_KEYDOWN) {
            SDL_Keycode key = e.key.keysym.sym;
            if (key == SDLK_RETURN) {
                Config::PLAY_SIDE = (selectedSide == 0) ? 1 : 2;
                return SideSelectStep::FINISHED;
            }
            if (key == SDLK_SPACE || key == SDLK_LEFT || key == SDLK_RIGHT) {
                selectedSide = 1 - selectedSide;
            }
        }
    }

    SDL_SetRenderDrawColor(ren, 10, 10, 30, 255);
    SDL_RenderClear(ren);

    SDL_Color white = {255, 255, 255, 255}, yellow = {255, 255, 0, 255}, gray = {120, 120, 120, 255}, cyan = {0, 255, 255, 255};

    renderer.drawText(ren, "SELECT PLAY SIDE", 640, 120, yellow, true, true);
    // ガイドテキストをシステムボタン基準に修正
    renderer.drawText(ren, "LEFT/RIGHT TO MOVE / DECIDE TO SELECT", 640, 180, gray, true, true);
    
    renderer.drawText(ren, "1P SIDE", 480, 360, (selectedSide == 0 ? white : gray), true, true);
    renderer.drawText(ren, "2P SIDE", 800, 360, (selectedSide == 1 ? white : gray), true, true);

    int lineX = (selectedSide == 0) ? 480 : 800;
    SDL_Rect cursor = { lineX - 70, 400, 140, 5 };
    SDL_SetRenderDrawColor(ren, cyan.r, cyan.g, cyan.b, 255);
    SDL_RenderFillRect(ren, &cursor);
    
    renderer.drawText(ren, "CURRENT SIDE: " + std::to_string(selectedSide + 1) + "P", 640, 550, cyan, true, true);

    SDL_RenderPresent(ren);
    return SideSelectStep::SELECTING;
}