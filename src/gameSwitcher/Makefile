INCLUDE_CJSON=1
include ../common/config.mk

CFILES := $(CFILES) \
	../common/utils/udp.c \
	../common/utils/retroarch_cmd.c \
	../bloomLaunch/bloom_launch.c \
	../bloomGameId/bloom_game_id.c \
	../bloomLibrary/bloom_library_query.c \
	../bloomLibrary/bloom_library_mutation.c \
	../bloomShell/bloom_shell_launch.c \
	../bloomShell/bloom_shell_settings.c \
	../playActivity/playActivityModel.c \
	../playActivity/playActivityMigrations.c

TARGET = gameSwitcher
LDFLAGS := $(LDFLAGS) -lSDL -lSDL_ttf -lSDL_image -lSDL_rotozoom -lpng -lsqlite3 -pthread

include ../common/commands.mk
include ../common/recipes.mk
