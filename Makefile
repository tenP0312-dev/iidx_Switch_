#---------------------------------------------------------------------------------
# .nroを作成するための Makefile (Windows対応版)
#---------------------------------------------------------------------------------
TARGET      := sdl2_red_square
BUILD       := build
OUTPUT      := $(BUILD)/$(TARGET)
SOURCES     := main.cpp BmsonLoader.cpp SoundManager.cpp NoteRenderer.cpp \
               SceneSelect.cpp ScenePlay.cpp SceneResult.cpp PlayEngine.cpp ScoreManager.cpp SceneTitle.cpp SceneDecision.cpp SceneSelectView.cpp SongManager.cpp ChartProjector.cpp JudgeManager.cpp SceneOption.cpp SceneModeSelect.cpp SceneSideSelect.cpp VirtualFolderManager.cpp BgaManager.cpp

# --- devkitProのパス設定 (自動取得) ---
ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO environment variable is not set. Please restart your terminal.")
endif

DEVKITA64   := $(DEVKITPRO)/devkitA64
LIBNX       := $(DEVKITPRO)/libnx
PORTLIBS    := $(DEVKITPRO)/portlibs/switch

# --- ツール類のパスを指定 ---
CC      := $(DEVKITA64)/bin/aarch64-none-elf-g++
ELF2NRO := $(DEVKITPRO)/tools/bin/elf2nro
NACPTOOL:= $(DEVKITPRO)/tools/bin/nacptool

# --- アイコンなどの設定 ---
ICON        := $(LIBNX)/default_icon.jpg
NACP        := $(OUTPUT).nacp
NACP_TITLE  := "SDL2 Red Square"
NACP_AUTHOR := "User"
NACP_VERSION:= "1.0.0"

# --- コンパイルオプション ---
CFLAGS  := -g -Wall -Os -ffunction-sections -fdata-sections -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS  += -D__SWITCH__
CFLAGS  += -I$(PORTLIBS)/include -I$(LIBNX)/include
CFLAGS  += -I$(PORTLIBS)/include/SDL2
CFLAGS  += -I$(PORTLIBS)/include/SDL2_mixer
CFLAGS  += -I$(PORTLIBS)/include/SDL2_ttf
CFLAGS  += -I.

# --- リンクオプション (最終確定版: SDLグループを先に、システムコアを最後に) ---
LDFLAGS := -specs=$(LIBNX)/switch.specs -g -march=armv8-a -mtune=cortex-a57 -fPIE
LDFLAGS += -L$(PORTLIBS)/lib -L$(LIBNX)/lib

# 1. 全てのSDL関連ライブラリをグループ化 (この中の相互依存を解決)
# 修正箇所: SDL2_imageの依存解決のため -ljpeg と -lwebp を追加
LDFLAGS += -Wl,--start-group -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lmodplug -lmpg123 -lvorbisfile -lopusfile -lvorbis -lopus -logg -lfreetype -lharfbuzz -lbz2 -lpng -ljpeg -lwebp -lz -Wl,--end-group

# 2. SDLグループが内部で依存するコアシステムライブラリを最後に配置 (libSDL2が依存するシンボルを解決)
LDFLAGS += -lEGL -lglapi -ldrm_nouveau -lnx -lm


.PHONY: all clean

all: $(OUTPUT).nro

$(OUTPUT).elf: $(SOURCES)
	@mkdir -p $(BUILD)
	@echo "Compiling..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(NACP):
	@echo "Creating NACP..."
	$(NACPTOOL) --create $(NACP_TITLE) $(NACP_AUTHOR) $(NACP_VERSION) $@

$(OUTPUT).nro: $(OUTPUT).elf $(NACP)
	@echo "Creating NRO..."
	$(ELF2NRO) $< $@ --icon=$(ICON) --nacp=$(NACP)
	@echo "Success! Output is in: $(OUTPUT).nro"

clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD)