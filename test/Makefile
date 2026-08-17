TEST = 1
INCLUDE_UTILS = 0
CFILES := ../src/infoPanel/imagesCache.c ../src/common/utils/log.c ../src/common/utils/file.c ../src/common/utils/str.c \
	../src/bloomLaunch/bloom_launch.c ../include/cjson/cJSON.c
include ../src/common/config.mk

TARGET = test
LDFLAGS := $(LDFLAGS) -L../lib -s -lSDL_image -lSDL -lSDL_rotozoom -lsqlite3 -lgtest -lgtest_main -lpthread

include ../src/common/commands.mk
include ../src/common/recipes.mk
