TEST = 1
INCLUDE_UTILS = 0
CFILES := ../src/infoPanel/imagesCache.c ../src/common/utils/log.c ../src/common/utils/file.c ../src/common/utils/str.c \
	../src/bloomLaunch/bloom_launch.c ../include/cjson/cJSON.c
CFILES := $(CFILES) ../src/infoPanel/imagesBrowser.c
CFILES := $(CFILES) ../src/infoPanel/imagesConfig.c
CFILES := $(CFILES) ../src/bloomSaveFlush/bloom_save_flush.c
CFILES := $(CFILES) ../src/bloomGameId/bloom_game_id.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra_database.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra_catalog.c
CFILES := $(CFILES) ../third-party/rcheevos/src/rc_compat.c ../third-party/rcheevos/src/rhash/md5.c \
	../third-party/rcheevos/src/rhash/hash.c ../third-party/rcheevos/src/rhash/hash_rom.c \
	../third-party/rcheevos/src/rhash/cdreader.c ../third-party/rcheevos/src/rhash/hash_disc.c \
	../third-party/rcheevos/src/rhash/hash_zip.c
CFILES := $(CFILES) ../src/playActivity/playActivityModel.c
CFILES := $(CFILES) ../src/playActivity/playActivityMigrations.c
CFILES := $(CFILES) ../src/playActivity/playActivityIdentity.c
CFILES := $(CFILES) ../src/playActivity/playActivityHealth.c
CFILES := $(CFILES) ../src/playActivity/playActivityDuration.c
CFILES := $(CFILES) ../src/gameSwitcher/gameSwitcherIdentity.c
CFILES := $(CFILES) ../src/common/utils/udp.c
include ../src/common/config.mk
CFLAGS += -I../third-party/rcheevos/include -I../third-party/rcheevos/src -DRC_HASH_NO_ENCRYPTED

TARGET = test
LDFLAGS := $(LDFLAGS) -L../lib -s -lSDL_image -lSDL -lSDL_rotozoom -lsqlite3 -lgtest -lgtest_main -lpthread

include ../src/common/commands.mk
include ../src/common/recipes.mk
