#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work_dir="${SCUMMVM_BUILD_DIR:-$repository_root/cache/scummvm-build}"
prefix="$work_dir/prefix"
package_dir="$repository_root/static/packages/RApp/SCUMM (ScummVM standalone)/RApp/scummvm"
jobs="${BUILD_JOBS:-2}"
toolchain=/opt/miyoomini-toolchain
export PATH="$toolchain/bin:$toolchain/usr/bin:$PATH"

scummvm_revision=0a8aa528e92836b86780ba0498dda3263fba19ea
ogg_revision=e1774cd77f471443541596e09078e78fdc342e4f
vorbis_revision=0657aee69dec8508a0011f47f3b69d7538e9d262
zlib_revision=51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf
libpng_revision=f135775ad4e5d4408d2e12ffcc71bb36e6b48551
freetype_revision=6a2b3e4007e794bfc6c91030d0ed987f925164a8
libjpeg_revision=9fc018fd1aa9598f21c9bc4d8d53c0cef007bdcf
giflib_revision=52b62de83d5facbbbde042b85bf3f61182e3bebd
libmad_revision=d6df2dd57747661e3562e22fd16dc410bec4ab0a
libmpeg2_revision=218a44b390b7d0e20e87e16a3b53c0176cd68b0e

# ScummVM embeds __DATE__ and __TIME__ in its version strings. GCC honors
# SOURCE_DATE_EPOCH for those macros, so use the pinned source commit time
# rather than allowing the wall clock to change the released executable.
export SOURCE_DATE_EPOCH=1701817090
export TZ=UTC

verify_revision() {
    test "$(git -C "$repository_root/third-party/$1" rev-parse HEAD)" = "$2"
}

verify_revision scummvm "$scummvm_revision"
verify_revision ogg "$ogg_revision"
verify_revision vorbis "$vorbis_revision"
verify_revision zlib "$zlib_revision"
verify_revision libpng "$libpng_revision"
verify_revision freetype "$freetype_revision"
verify_revision libjpeg-turbo "$libjpeg_revision"
verify_revision giflib "$giflib_revision"
verify_revision libmad "$libmad_revision"
verify_revision libmpeg2 "$libmpeg2_revision"

rm -rf "$work_dir"
mkdir -p "$work_dir" "$prefix"

export CC=arm-linux-gnueabihf-gcc
export CXX=arm-linux-gnueabihf-g++
export AR=arm-linux-gnueabihf-ar
export RANLIB=arm-linux-gnueabihf-ranlib
export STRIP=arm-linux-gnueabihf-strip
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig:$prefix/share/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"

cat > "$work_dir/toolchain.cmake" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_AR arm-linux-gnueabihf-ar)
set(CMAKE_RANLIB arm-linux-gnueabihf-ranlib)
set(CMAKE_FIND_ROOT_PATH "$prefix")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

archive_source() {
    name="$1"
    revision="$2"
    destination="$work_dir/$name-source"
    mkdir -p "$destination"
    git -C "$repository_root/third-party/$name" archive "$revision" | tar -x -C "$destination"
}

cmake_static() {
    name="$1"
    shift
    cmake -S "$work_dir/$name-source" -B "$work_dir/$name-build" \
        -DCMAKE_TOOLCHAIN_FILE="$work_dir/toolchain.cmake" \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF "$@"
    cmake --build "$work_dir/$name-build" --parallel "$jobs"
    cmake --build "$work_dir/$name-build" --target install
}

archive_source zlib "$zlib_revision"
cmake_static zlib -DZLIB_BUILD_EXAMPLES=OFF

archive_source ogg "$ogg_revision"
cmake_static ogg -DBUILD_TESTING=OFF -DINSTALL_DOCS=OFF -DINSTALL_PKG_CONFIG_MODULE=ON

archive_source vorbis "$vorbis_revision"
cmake_static vorbis -DOGG_ROOT="$prefix" -DBUILD_TESTING=OFF

archive_source libpng "$libpng_revision"
cmake_static libpng -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF \
    -DZLIB_ROOT="$prefix"

archive_source freetype "$freetype_revision"
cmake_static freetype -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE \
    -DFT_DISABLE_HARFBUZZ=TRUE -DFT_DISABLE_PNG=TRUE -DFT_DISABLE_ZLIB=TRUE

archive_source libjpeg-turbo "$libjpeg_revision"
mkdir -p "$work_dir/libjpeg-build"
cd "$work_dir/libjpeg-build"
"$work_dir/libjpeg-turbo-source/configure" --host=arm-linux-gnueabihf \
    --prefix="$prefix" --disable-shared --enable-static
make -j"$jobs"
make install

archive_source giflib "$giflib_revision"
mkdir -p "$work_dir/giflib-build" "$prefix/include" "$prefix/lib"
gif_objects=
for source in dgif_lib egif_lib gifalloc gif_err gif_font gif_hash openbsd-reallocarray; do
    "$CC" -O2 -fPIC -I"$work_dir/giflib-source" -c \
        "$work_dir/giflib-source/$source.c" -o "$work_dir/giflib-build/$source.o"
    gif_objects="$gif_objects $work_dir/giflib-build/$source.o"
done
"$AR" rcs "$prefix/lib/libgif.a" $gif_objects
install -m 0644 "$work_dir/giflib-source/gif_lib.h" "$prefix/include/"

archive_source libmad "$libmad_revision"
mkdir -p "$work_dir/libmad-build"
mad_objects=
for source in version fixed bit timer stream frame synth decoder layer12 layer3 huffman; do
    "$CC" -O2 -fPIC -DFPM_ARM -DOPT_SPEED -I"$work_dir/libmad-source" -c \
        "$work_dir/libmad-source/$source.c" -o "$work_dir/libmad-build/$source.o"
    mad_objects="$mad_objects $work_dir/libmad-build/$source.o"
done
"$AR" rcs "$prefix/lib/libmad.a" $mad_objects
install -m 0644 "$work_dir/libmad-source/mad.h" "$prefix/include/"

archive_source libmpeg2 "$libmpeg2_revision"
mkdir -p "$work_dir/libmpeg2-build"
cd "$work_dir/libmpeg2-build"
"$work_dir/libmpeg2-source/configure" --host=arm-linux-gnueabihf \
    --prefix="$prefix" --disable-shared --enable-static --disable-sdl
make -j"$jobs"
make install

archive_source scummvm "$scummvm_revision"
# All optional libraries are linked statically into the replacement. CMake's
# zlib build installs both forms, so remove its shared form before configure.
find "$prefix/lib" -maxdepth 1 -type f -name '*.so*' -delete
find "$prefix/lib" -maxdepth 1 -type l -name '*.so*' -delete
cd "$work_dir/scummvm-source"
CPPFLAGS="-I$prefix/include -I$prefix/include/freetype2" \
LDFLAGS="-L$prefix/lib" \
    ./configure --host=miyoomini --enable-release --enable-engine=testbed \
        --enable-ext-neon \
        --with-ogg-prefix="$prefix" \
        --with-vorbis-prefix="$prefix" \
        --with-mad-prefix="$prefix" \
        --with-zlib-prefix="$prefix" \
        --with-mpeg2-prefix="$prefix" \
        --with-freetype2-prefix="$prefix" \
        --with-jpeg-prefix="$prefix" \
        --with-png-prefix="$prefix" \
        --with-gif-prefix="$prefix"
make -j"$jobs" scummvm

test -s scummvm
"$STRIP" -o scummvm.stripped scummvm
arm-linux-gnueabihf-readelf -h scummvm.stripped | grep -F 'Machine:' | grep -Fq 'ARM'
arm-linux-gnueabihf-readelf -d scummvm.stripped | grep -Fq 'Shared library: [libSDL-1.2.so.0]'
for inherited_library in libvorbisfile libmad libjpeg libpng libgif libmpeg2 libfreetype; do
    if arm-linux-gnueabihf-readelf -d scummvm.stripped | grep -Fq "Shared library: [$inherited_library"; then
        printf 'unexpected dynamic codec dependency: %s\n' "$inherited_library" >&2
        exit 1
    fi
done

install -m 0755 scummvm.stripped "$work_dir/scummvm"
install -m 0755 scummvm.stripped "$package_dir/scummvm"
rm -f "$package_dir"/libfreetype.so.6 \
    "$package_dir"/libgif.so.7 \
    "$package_dir"/libmad.so.0 \
    "$package_dir"/libmpeg2.so.0 \
    "$package_dir"/libogg.so.0 \
    "$package_dir"/libpng16.so.16 \
    "$package_dir"/libtheoradec.so.1 \
    "$package_dir"/libvorbis.so.0 \
    "$package_dir"/libvorbisfile.so.3 \
    "$package_dir"/libz.so.1
{
    printf '# ScummVM and statically linked dependency licenses\n\n'
    printf '## ScummVM\n\n'
    cat "$work_dir/scummvm-source/COPYING"
    printf '\n## libogg\n\n'
    cat "$work_dir/ogg-source/COPYING"
    printf '\n## libvorbis\n\n'
    cat "$work_dir/vorbis-source/COPYING"
    printf '\n## zlib\n\n'
    cat "$work_dir/zlib-source/LICENSE"
    printf '\n## libpng\n\n'
    cat "$work_dir/libpng-source/LICENSE"
    printf '\n## FreeType\n\n'
    cat "$work_dir/freetype-source/docs/FTL.TXT"
    printf '\n## Independent JPEG Group libjpeg\n\n'
    cat "$work_dir/libjpeg-turbo-source/README"
    printf '\n## giflib\n\n'
    cat "$work_dir/giflib-source/COPYING"
    printf '\n## libmad\n\n'
    cat "$work_dir/libmad-source/COPYING"
    printf '\n## libmpeg2\n\n'
    cat "$work_dir/libmpeg2-source/COPYING"
} > "$package_dir/LICENSE.MD"
printf '%s\n' "$work_dir/scummvm"
