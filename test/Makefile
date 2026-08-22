TEST = 1
INCLUDE_UTILS = 0
CFILES := ../src/infoPanel/imagesCache.c ../src/common/utils/log.c ../src/common/utils/file.c ../src/common/utils/str.c \
	../src/bloomLaunch/bloom_launch.c ../include/cjson/cJSON.c
CFILES := $(CFILES) ../src/infoPanel/imagesBrowser.c
CFILES := $(CFILES) ../src/infoPanel/imagesConfig.c
CFILES := $(CFILES) ../src/bloomSaveFlush/bloom_save_flush.c
CFILES := $(CFILES) ../src/bloomGameId/bloom_game_id.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra_archive.c
CFILES := $(CFILES) ../third-party/zlib/contrib/minizip/unzip.c ../third-party/zlib/contrib/minizip/ioapi.c
CFILES := $(CFILES) ../third-party/zlib/adler32.c ../third-party/zlib/crc32.c ../third-party/zlib/inffast.c
CFILES := $(CFILES) ../third-party/zlib/inflate.c ../third-party/zlib/inftrees.c ../third-party/zlib/zutil.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra_database.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra_catalog.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra_scanner.c
CFILES := $(CFILES) ../src/bloomRa/bloom_ra_account.c
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
CFILES := $(CFILES) ../src/gameSwitcher/gameSwitcherAchievements.c
CFILES := $(CFILES) ../src/gameSwitcher/gameSwitcherLibrary.c
CFILES := $(CFILES) ../src/bloomUi/bloom_ui_core.c
CFILES := $(CFILES) ../src/bloomUi/bloom_ui_renderer.c
CFILES := $(CFILES) ../src/bloomUi/bloom_ui_input.c
CFILES := $(CFILES) ../src/bloomShell/bloom_shell_launch.c
CFILES := $(CFILES) ../src/bloomShell/bloom_shell_status.c
CFILES := $(CFILES) ../src/bloomShell/bloom_shell_settings.c
CFILES := $(CFILES) ../src/bloomShell/bloom_shell_ra_form.c
CFILES := $(CFILES) ../src/bloomShell/bloom_shell_root.c
CFILES := $(CFILES) ../src/bloomSettings/bloom_settings.c
CFILES := $(CFILES) ../src/bloomLibrary/bloom_library_database.c
CFILES := $(CFILES) ../src/bloomLibrary/bloom_library_import.c
CFILES := $(CFILES) ../src/bloomLibrary/bloom_library_scan.c
CFILES := $(CFILES) ../src/bloomLibrary/bloom_library_query.c
CFILES := $(CFILES) ../src/bloomLibrary/bloom_library_legacy.c
CFILES := $(CFILES) ../src/common/utils/udp.c
include ../src/common/config.mk
CFLAGS += -I. -DBLOOM_TEST=1 -I../third-party/rcheevos/include -I../third-party/rcheevos/src \
	-I../third-party/zlib -I../third-party/zlib/contrib/minizip -DRC_HASH_NO_ENCRYPTED
CXXFLAGS += -I. -DBLOOM_TEST=1

TARGET = test
LDFLAGS := $(LDFLAGS) -L../lib -s -lSDL_image -lSDL -lSDL_rotozoom -lsqlite3 -lgtest -lgtest_main -lpthread
LDFLAGS := $(LDFLAGS) -ldl

include ../src/common/commands.mk
include ../src/common/recipes.mk
