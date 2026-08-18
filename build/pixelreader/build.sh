#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
work="$root/build/pixelreader/work"
prefix="$work/prefix"
pixel_source="$work/pixel-reader-src"
zlib_source="$work/zlib-src"
libxml2_source="$work/libxml2-src"
libzip_source="$work/libzip-src"
package="$root/static/packages/App/Ebook Reader (PixelReader)/App/PixelReader"
cross=arm-linux-gnueabihf-
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')}

export SOURCE_DATE_EPOCH=1694360870
export CC="${cross}gcc"
export CXX="${cross}g++"
export AR="${cross}ar"
export RANLIB="${cross}ranlib"
export STRIP="${cross}strip"
export CFLAGS="-O2 -ffile-prefix-map=$root=. -fdebug-prefix-map=$root=."
export CXXFLAGS="$CFLAGS"

rm -rf "$work"
mkdir -p "$prefix"
for source_pair in \
    "$root/third-party/pixel-reader:$pixel_source" \
    "$root/third-party/zlib:$zlib_source" \
    "$root/third-party/libxml2:$libxml2_source" \
    "$root/third-party/libzip:$libzip_source"
do
    source_path=${source_pair%%:*}
    destination_path=${source_pair#*:}
    mkdir -p "$destination_path"
    (cd "$source_path" && tar --exclude=.git -cf - .) | (cd "$destination_path" && tar -xf -)
done

cmake -S "$zlib_source" -B "$work/zlib" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_SHARED_LIBS=OFF
cmake --build "$work/zlib" --target zlibstatic --parallel "$jobs"
mkdir -p "$prefix/lib" "$prefix/include"
install -m 0644 "$work/zlib/libz.a" "$prefix/lib/libz.a"
install -m 0644 "$work/zlib/zconf.h" "$zlib_source/zlib.h" "$prefix/include/"

sed -i '1s/VERSION 3\.18/VERSION 3.13/' "$libxml2_source/CMakeLists.txt"
sed -i '/include(CheckLinkerFlag)/d' "$libxml2_source/CMakeLists.txt"
sed -i 's/check_linker_flag(C "LINKER:--undefined-version" FLAG_UNDEFINED_VERSION)/set(FLAG_UNDEFINED_VERSION FALSE)/' "$libxml2_source/CMakeLists.txt"
cmake -S "$libxml2_source" -B "$work/libxml2" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_SHARED_LIBS=OFF \
    -DLIBXML2_WITH_ICONV=OFF \
    -DLIBXML2_WITH_LZMA=OFF \
    -DLIBXML2_WITH_ZLIB=OFF \
    -DLIBXML2_WITH_HTTP=OFF \
    -DLIBXML2_WITH_MODULES=OFF \
    -DLIBXML2_WITH_PROGRAMS=OFF \
    -DLIBXML2_WITH_PYTHON=OFF \
    -DLIBXML2_WITH_TESTS=OFF
cmake --build "$work/libxml2" --parallel "$jobs"
cmake --build "$work/libxml2" --target install

cmake -S "$libzip_source" -B "$work/libzip" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TOOLS=OFF \
    -DBUILD_REGRESS=OFF \
    -DBUILD_OSSFUZZ=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_DOC=OFF \
    -DENABLE_BZIP2=OFF \
    -DENABLE_LZMA=OFF \
    -DENABLE_ZSTD=OFF \
    -DENABLE_OPENSSL=OFF \
    -DZLIB_LIBRARY="$prefix/lib/libz.a" \
    -DZLIB_INCLUDE_DIR="$prefix/include"
cmake --build "$work/libzip" --parallel "$jobs"
cmake --build "$work/libzip" --target install

make -C "$pixel_source" clean
make -C "$pixel_source" -j"$jobs" build/reader \
    PLATFORM=miyoomini \
    CROSS_COMPILE="$cross" \
    PREFIX="$prefix" \
    CXXFLAGS="-std=c++17 -O2 -DPLATFORM_MIYOO_MINI=1 -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve+simd -ffile-prefix-map=$root=. -fdebug-prefix-map=$root=. -Wno-psabi" \
    INCLUDE="-Isrc -I$prefix/include -I$prefix/include/libxml2" \
    LDFLAGS="-lstdc++ -lSDL -lSDL_ttf -lSDL_image $prefix/lib/libzip.a $prefix/lib/libxml2.a $prefix/lib/libz.a -ldl -lm -pthread -lstdc++fs"

"$STRIP" --strip-unneeded "$pixel_source/build/reader"
install -m 0755 "$pixel_source/build/reader" "$package/reader"
rm -rf "$package/lib"
install -m 0644 "$pixel_source/LICENSE" "$package/LICENSE.PixelReader"
install -m 0644 "$zlib_source/LICENSE" "$package/LICENSE.zlib"
install -m 0644 "$libxml2_source/Copyright" "$package/LICENSE.libxml2"
install -m 0644 "$libzip_source/LICENSE" "$package/LICENSE.libzip"
