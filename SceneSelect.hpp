#ifndef SCENESELECT_HPP
#define SCENESELECT_HPP

#include <vector>
#include <string>
#include <SDL2/SDL.h>
#include "NoteRenderer.hpp"
#include "BMSData.hpp"
#include "CommonTypes.hpp"

struct SongEntry {
    std::string filename;
    std::string title;
    std::string subtitle;
    std::string artist;
    std::string chartName;
    double bpm;
    int level;
    double total;
    int totalNotes;
    ClearType clearType;
    int exScore;
    int maxCombo;
    std::string rank;
    std::string modeHint;
    // --- 【追加】プレビュー再生用のパス情報 ---
    std::string previewPath; 
    std::string rootDir;     
};

struct SongGroup {
    std::string title;
    std::string sortName;
    std::vector<int> songIndices;
    int currentDiffIdx = 0;
    bool isFolder = false;
    std::string folderPath = "";
    std::string customFont = "";
    std::string customLogo = "";
    std::string customSE = "";
};

enum class SelectState {
    SELECT_SONG,
    EDIT_OPTION,
    EDIT_DETAIL
};

enum class SortMode {
    ALPHABET,
    LEVEL,
    CLEAR_LAMP,
    SCORE,
    BPM,
    MAX_COUNT
};

class SceneSelect {
public:
    static int getDifficultyOrder(const SongEntry& entry); 
    void init(bool forceScan, SDL_Renderer* ren, NoteRenderer& renderer, int currentStage); 
    std::string update(SDL_Renderer* ren, NoteRenderer& renderer, int currentStage, bool& quit);
    bool shouldBackToModeSelect() const { return backToModeSelectRequested; }

public:
    SelectState currentState = SelectState::SELECT_SONG;
    SortMode currentSort = SortMode::ALPHABET;
    std::vector<SongEntry> songCache;
    std::vector<SongGroup> songGroups;
    int selectedIndex = 0;
    int preferredDifficultySlot = 1; 
    int lastSelectedIndex = -1; 
    int lastDifficultyIdx = -1;
    BMSHeader cachedHeader;
    BestScore cachedBest;
    bool isShowingExitDialog = false;    
    int exitDialogSelection = 0;         
    bool startButtonPressed = false;     
    bool effectButtonPressed = false;    

    bool isExtraFolderSelected() const;
    bool isOneMoreFolderSelected() const;

    bool scrUpPressed = false;    
    bool scrDownPressed = false;  
    Uint32 lastScrollTime = 0;    

    int optionCategoryIndex = 0; 
    int detailOptionIndex = 0;   

private:
    void prepareSongList(bool forceScan, SDL_Renderer* ren, NoteRenderer& renderer, int currentStage); 
    void syncDifficultyWithPreferredSlot();
    void applySort();
    void renderOptionOverlay(SDL_Renderer* ren, NoteRenderer& renderer);
    void renderExitDialog(SDL_Renderer* ren, NoteRenderer& renderer);

    bool backToModeSelectRequested = false; 
    // ★追加：決定時のフォルダ属性を保持するフラグ
    bool lastSelectedWasExtra = false;
    bool lastSelectedWasOneMore = false;

    // --- 【追加】プレビュー制御用変数 ---
    int previewTimer = 0;
};

#endif