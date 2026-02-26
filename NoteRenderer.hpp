#ifndef NOTERENDERER_HPP
#define NOTERENDERER_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <list>
#include <vector>
#include <map>
#include "BmsonLoader.hpp"
#include "CommonTypes.hpp"

/**
 * @brief テクスチャとサイズ情報をペアで管理し、SDL_QueryTextureを不要にする
 */
struct TextureRegion {
    SDL_Texture* texture = nullptr;
    int w = 0;
    int h = 0;

    // if (region) でテクスチャの有無を確認できるようにする
    explicit operator bool() const { return texture != nullptr; }

    void reset() {
        if (texture) SDL_DestroyTexture(texture);
        texture = nullptr;
        w = h = 0;
    }
};

// --- キャッシュキーとハッシュ関数 (変更なし) ---
struct TextCacheKey {
    std::string text;
    uint32_t color_rgba;
    bool isBig;
    std::string fontPath;
    bool operator==(const TextCacheKey& other) const {
        return color_rgba == other.color_rgba && isBig == other.isBig && text == other.text && fontPath == other.fontPath;
    }
};

struct TextCacheKeyHash {
    std::size_t operator()(const TextCacheKey& k) const {
        std::size_t h = std::hash<std::string>{}(k.text);
        h ^= std::hash<uint32_t>{}(k.color_rgba) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>{}(k.isBig) + 0x9e3779b9 + (h << 6) + (h >> 2);
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
    
    // PlayableNote を const 参照で受ける (コピー根絶)
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

    struct CacheEntry {
        SDL_Texture* texture;
        int w, h;
        std::list<TextCacheKey>::iterator lruIt;
    };
    std::unordered_map<TextCacheKey, CacheEntry, TextCacheKeyHash> textTextureCache;
    std::list<TextCacheKey> lruList;
    const size_t MAX_TEXT_CACHE = 256;

    std::map<std::string, TextureRegion> textureCache; // mapの中身もサイズ持ちにする
    std::map<std::string, TTF_Font*> customFontCache;

    // --- テクスチャ群を TextureRegion に置き換え ---
    TextureRegion lane_Flame, lane_Flame2;
    TextureRegion texBackground;
    TextureRegion texNoteWhite, texNoteBlue, texNoteRed;
    TextureRegion texNoteWhite_LN, texNoteWhite_LN_Active1, texNoteWhite_LN_Active2;
    TextureRegion texNoteBlue_LN, texNoteBlue_LN_Active1, texNoteBlue_LN_Active2;
    TextureRegion texNoteRed_LN, texNoteRed_LN_Active1, texNoteRed_LN_Active2;
    TextureRegion texNoteWhite_LNS, texNoteWhite_LNE;
    TextureRegion texNoteBlue_LNS, texNoteBlue_LNE;
    TextureRegion texNoteRed_LNS, texNoteRed_LNE;
    TextureRegion texKeybeamWhite, texKeybeamBlue, texKeybeamRed;
    TextureRegion texJudgeAtlas, texNumberAtlas;
    std::vector<TextureRegion> texBombs;
    TextureRegion texLaneCover, texGaugeAssist, texGaugeNormal, texGaugeHard, texGaugeExHard, texGaugeHazard, texGaugeDan;
    TextureRegion texKeys, tex_scratch;

    // ヘルパー: ロード時にサイズを自動取得して格納する
    void loadAndCache(SDL_Renderer* ren, TextureRegion& region, const std::string& path);
};

#endif