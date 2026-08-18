#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
output="$root/lib"
sqlite_source="$root/include/sqlite3/sqlite3.c"
rotozoom_source="$root/include/SDL/SDL_rotozoom.c"

cc=${CROSS_COMPILE:-arm-linux-gnueabihf-}gcc
strip=${CROSS_COMPILE:-arm-linux-gnueabihf-}strip
common_flags="-O2 -fPIC -ffile-prefix-map=$root=. -fdebug-prefix-map=$root=."

mkdir -p "$output"

"$cc" $common_flags -shared -Wl,--build-id=none \
    -Wl,-soname,libsqlite3.so.0 \
    -o "$output/libsqlite3.so.0.new" "$sqlite_source" -ldl -lpthread
"$strip" --strip-unneeded "$output/libsqlite3.so.0.new"
cp "$output/libsqlite3.so.0.new" "$output/libsqlite3.so.new"

"$cc" $common_flags -shared -Wl,--build-id=none \
    -Wl,-soname,libSDL_rotozoom.so \
    -o "$output/libSDL_rotozoom.so.new" "$rotozoom_source"
"$strip" --strip-unneeded "$output/libSDL_rotozoom.so.new"

mv "$output/libsqlite3.so.0.new" "$output/libsqlite3.so.0"
mv "$output/libsqlite3.so.new" "$output/libsqlite3.so"
mv "$output/libSDL_rotozoom.so.new" "$output/libSDL_rotozoom.so"
