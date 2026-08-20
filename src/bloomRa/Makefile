INCLUDE_UTILS = 0
TARGET = bloom-ra
CFILES := ../bloomGameId/bloom_game_id.c \
	../../third-party/zlib/contrib/minizip/unzip.c \
	../../third-party/zlib/contrib/minizip/ioapi.c \
	../../third-party/zlib/adler32.c \
	../../third-party/zlib/crc32.c \
	../../third-party/zlib/inffast.c \
	../../third-party/zlib/inflate.c \
	../../third-party/zlib/inftrees.c \
	../../third-party/zlib/zutil.c \
	../../include/cjson/cJSON.c \
	../../third-party/rcheevos/src/rc_compat.c \
	../../third-party/rcheevos/src/rhash/md5.c \
	../../third-party/rcheevos/src/rhash/hash.c \
	../../third-party/rcheevos/src/rhash/hash_rom.c \
	../../third-party/rcheevos/src/rhash/cdreader.c \
	../../third-party/rcheevos/src/rhash/hash_disc.c \
	../../third-party/rcheevos/src/rhash/hash_zip.c

include ../common/config.mk
CFLAGS += -I../../third-party/rcheevos/include -I../../third-party/rcheevos/src \
	-I../../third-party/zlib -I../../third-party/zlib/contrib/minizip -DRC_HASH_NO_ENCRYPTED
LDFLAGS += -lpthread
LDFLAGS += -lsqlite3
LDFLAGS += -Wl,-rpath,/mnt/SDCARD/.tmp_update/lib
include ../common/commands.mk
include ../common/recipes.mk
