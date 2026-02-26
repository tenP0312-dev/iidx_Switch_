#ifndef SCENEOPTION_HPP
#define SCENEOPTION_HPP

#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include "CommonTypes.hpp"
#include "NoteRenderer.hpp"
#include "Config.hpp"

enum class OptionState {
    SELECTING_ITEM,  // どの設定を変えるか選択中
    ADJUSTING_VALUE, // 数値変更専用モード
    WAITING_KEY,     // キーコンフィグの入力待ち
    FINISHED         // オプション終了
};

struct OptionMenuItem {
    std::string label;
    std::string current_value;

    // エラー回避のためコンストラクタを明示的に定義
    OptionMenuItem(std::string l, std::string v) : label(l), current_value(v) {}
};

class SceneOption {
public:
    void init();
    OptionState update(SDL_Renderer* ren, NoteRenderer& renderer);

private:
    OptionState state = OptionState::SELECTING_ITEM;
    int cursor = 0;
    int configStep = 0; 
    uint32_t lastConfigTime = 0; 
    uint32_t repeatTimer = 0;
    std::vector<OptionMenuItem> items;

    void updateItemList();
    void handleKeyConfig(int btn);
};

#endif