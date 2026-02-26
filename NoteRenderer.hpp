#ifndef NOTERENDERER_HPP
#define NOTERENDERER_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <list>
#include <vector>
#include "BmsonLoader.hpp"
#include "CommonTypes.hpp"

/**
 * @brief キャッシュキー構造体: 文字列結合を排除するためのデータ指向設計
 */
struct TextCacheKey {
    std::string text;
    uint32_t color_rgba;
    bool isBig;
    std::string fontPath;

    bool operator==(const TextCacheKey& other) const {
        return color_rgba == other.color_rgba &&
               isBig == other.isBig &&
               text == other.text &&
               fontPath == other.fontPath;
    }
};

/**
 * @brief TextCacheKey用のハッシュ関数
 */
struct TextCacheKeyHash {
    std::size_t operator()(const TextCacheKey& k) const {
        std::size_t h = std::hash<std::string>{}(k.text);
        // FNV-1a的な結合
        h ^= std::hash<uint32_t>{}(k.color_rgba) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>{}(k.isBig) + 0x9e3779b9 + (h << 6) + (h >> 2);
        if(!k.fontPath.empty()) h ^= std::hash<std::string>{}(k.fontPath);
        return h;
    }
};

class NoteRenderer {
public:
    void init(SDL_Renderer* ren); 
    void cleanup();
    void clearTextCache();

    void drawText(SDL_Renderer* ren, const std::string& text, int x, int y, SDL_Color color, bool isBig, bool isCenter = false, bool isRight = false, const std::string& fontPath = "");
    void drawTextCached(SDL_Renderer* ren, const std::string& text, int x, int y, SDL_Color color, bool isBig, bool isCenter = false, bool isRight = false, const std::string& fontPath = "");
    void drawImage(SDL_Renderer* ren, const std::string& path, int x, int y, int w, int h, int alpha = 255);
    
    // シグネチャ変更：PlayableNote を const 参照で受ける（コピー抑制）
    void renderNote(SDL_Renderer* ren, const PlayableNote& note, double cur_ms, double speed, bool isAuto = false);
    
    void renderBackground(SDL_Renderer* ren);
    void renderLanes(SDL_Renderer* ren, double progress);
    void renderBeatLine(SDL_Renderer* ren, double diff, double speed);
    void renderHitEffect(SDL_Renderer* ren, int lane, float progress);
    void renderBomb(SDL_Renderer* ren, int lane, int frame);
    void renderJudgment(SDL_Renderer* ren, const std::string& text, float progress, SDL_Color color, int combo = 0);
    void renderCombo(SDL_Renderer* ren, int combo);
    void renderGauge(SDL_Renderer* ren, double gaugeValue, int gaugeOption, bool isFailed);
    void renderUI(SDL_Renderer* ren, const BMSHeader& header, int fps, double bpm, int exScore);
    void renderDecisionInfo(SDL_Renderer* ren, const BMSHeader& header);
    void renderLoading(SDL_Renderer* ren, int current, int total, const std::string& filename);
    void renderResult(SDL_Renderer* ren, const PlayStatus& status, const BMSHeader& header, const std::string& rank);

private:
    TTF_Font* fontSmall = nullptr;
    TTF_Font* fontBig = nullptr;

    // LRUキャッシュ用データ構造
    struct CacheEntry {
        SDL_Texture* texture;
        int w, h;
        std::list<TextCacheKey>::iterator lruIt;
    };
    std::unordered_map<TextCacheKey, CacheEntry, TextCacheKeyHash> textTextureCache;
    std::list<TextCacheKey> lruList;
    const size_t MAX_TEXT_CACHE = 256; // 128から倍増（LRUなので安全）

    std::map<std::string, SDL_Texture*> textureCache;
    std::map<std::string, TTF_Font*> customFontCache;

    // --- テクスチャ群 ---
    SDL_Texture *lane_Flame = nullptr, *lane_Flame2 = nullptr;
    SDL_Texture *texBackground = nullptr;
    SDL_Texture *texNoteWhite = nullptr, *texNoteBlue = nullptr, *texNoteRed = nullptr;
    SDL_Texture *texNoteWhite_LN = nullptr, *texNoteWhite_LN_Active1 = nullptr, *texNoteWhite_LN_Active2 = nullptr;
    SDL_Texture *texNoteBlue_LN = nullptr, *texNoteBlue_LN_Active1 = nullptr, *texNoteBlue_LN_Active2 = nullptr;
    SDL_Texture *texNoteRed_LN = nullptr, *texNoteRed_LN_Active1 = nullptr, *texNoteRed_LN_Active2 = nullptr;
    SDL_Texture *texNoteWhite_LNS = nullptr, *texNoteWhite_LNE = nullptr;
    SDL_Texture *texNoteBlue_LNS = nullptr, *texNoteBlue_LNE = nullptr;
    SDL_Texture *texNoteRed_LNS = nullptr, *texNoteRed_LNE = nullptr;
    SDL_Texture *texKeybeamWhite = nullptr, *texKeybeamBlue = nullptr, *texKeybeamRed = nullptr;
    SDL_Texture *texJudgeAtlas = nullptr, *texNumberAtlas = nullptr;
    std::vector<SDL_Texture*> texBombs;
    SDL_Texture *texLaneCover = nullptr, *texGaugeAssist = nullptr, *texGaugeNormal = nullptr;
    SDL_Texture *texGaugeHard = nullptr, *texGaugeExHard = nullptr, *texGaugeHazard = nullptr, *texGaugeDan = nullptr;
    SDL_Texture *texKeys = nullptr, *tex_scratch = nullptr;
};

#endif