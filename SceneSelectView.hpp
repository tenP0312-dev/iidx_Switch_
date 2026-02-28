#ifndef SCENESELECTVIEW_HPP
#define SCENESELECTVIEW_HPP

#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include "NoteRenderer.hpp"
#include "ScoreManager.hpp"
#include "BmsonLoader.hpp"

// 既存のSceneSelectで使われている構造体や列挙型への依存を解決
struct SongEntry;
struct SongGroup;

class SceneSelectView {
public:
    /**
     * @brief 選曲画面の描画
     * currentStage を利用して現在のプレイ回数を表示します。
     */
    void render(SDL_Renderer* ren, NoteRenderer& renderer, 
                const std::vector<SongGroup>& songGroups, 
                const std::vector<SongEntry>& songCache,
                int selectedIndex, int currentState, 
                bool isShowingExitDialog, int exitDialogSelection,
                int categoryIndex = 0,
                int detailIndex = 0,
                int currentStage = 1);

private:
    void renderOptionOverlay(SDL_Renderer* ren, NoteRenderer& renderer, int categoryIndex);
    void renderDetailOptionOverlay(SDL_Renderer* ren, NoteRenderer& renderer, int detailIndex);
    void renderExitDialog(SDL_Renderer* ren, NoteRenderer& renderer, int selection);
};

#endif



