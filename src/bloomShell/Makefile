INCLUDE_CJSON=1
INCLUDE_UTILS=0
include ../common/config.mk

CFILES := $(CFILES) ../bloomUi/bloom_ui_core.c ../bloomUi/bloom_ui_input.c \
	../bloomUi/bloom_ui_renderer.c ../bloomLibrary/bloom_library_query.c \
	../gameSwitcher/gameSwitcherLibrary.c \
	../bloomLaunch/bloom_launch.c ../bloomGameId/bloom_game_id.c
TARGET = bloom-shell
LDFLAGS := $(LDFLAGS) -lSDL -lSDL_ttf -lsqlite3

include ../common/commands.mk
include ../common/recipes.mk
