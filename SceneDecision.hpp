#ifndef SCENEDECISION_HPP
#define SCENEDECISION_HPP

#include <SDL2/SDL.h>
#include "NoteRenderer.hpp"
#include "BmsonLoader.hpp"

class SceneDecision {
public:
    /**
     * @brief 決定画面を実行します。
     * @return true: プレイ開始, false: キャンセルして選曲に戻る
     */
    bool run(SDL_Renderer* ren, NoteRenderer& renderer, const BMSHeader& header);
};

#endif