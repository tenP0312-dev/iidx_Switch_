#ifndef SCENETITLE_HPP
#define SCENETITLE_HPP

#include <SDL2/SDL.h>
#include "NoteRenderer.hpp"

enum class TitleStep {
    PRESS_START,
    FINISHED
};

class SceneTitle {
public:
    void init();
    TitleStep update(SDL_Renderer* ren, NoteRenderer& renderer);

private:
    TitleStep currentStep = TitleStep::PRESS_START;
};

#endif



