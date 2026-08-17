#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work_dir="${OPENBOR_BUILD_DIR:-$repository_root/cache/openbor-build}"
source_dir="$work_dir/source"
sdl_source_dir="$work_dir/sdl2-source"
sdl_build_dir="$work_dir/sdl2"
sdl_install_dir="$work_dir/sdl2-install"
codec_install_dir="$work_dir/codecs-install"
package_dir="$repository_root/static/packages/RApp/Game engine - Open Beats of Rage/RApp/OpenBOR"

openbor_revision=b00efbc7752cb55709dfc9fdfdfc7cfe78ddfb90
sdl_revision=b424665e0899769b200231ba943353a5fee1b6b6
ogg_revision=e1774cd77f471443541596e09078e78fdc342e4f
tremor_revision=820fb3237ea81af44c9cc468c8b4e20128e3e5ad

test "$(git -C "$repository_root/third-party/openbor" rev-parse HEAD)" = "$openbor_revision"
test "$(git -C "$repository_root/third-party/SDL2" rev-parse HEAD)" = "$sdl_revision"
test "$(git -C "$repository_root/third-party/ogg" rev-parse HEAD)" = "$ogg_revision"
test "$(git -C "$repository_root/third-party/tremor" rev-parse HEAD)" = "$tremor_revision"

rm -rf "$source_dir"
mkdir -p "$source_dir"
git -C "$repository_root/third-party/openbor" archive "$openbor_revision" | tar -x -C "$source_dir"
patch -d "$source_dir" -p1 < "$repository_root/build/openbor/openbor-mmiyoo.patch"

if ! test -f "$sdl_install_dir/.revision" || ! grep -Fxq "$sdl_revision" "$sdl_install_dir/.revision"; then
    rm -rf "$sdl_source_dir" "$sdl_build_dir" "$sdl_install_dir"
    mkdir -p "$sdl_source_dir" "$sdl_build_dir" "$sdl_install_dir"
    git -C "$repository_root/third-party/SDL2" archive "$sdl_revision" | tar -x -C "$sdl_source_dir"
    cd "$sdl_build_dir"
    CC=arm-linux-gnueabihf-gcc \
    AR=arm-linux-gnueabihf-ar \
    RANLIB=arm-linux-gnueabihf-ranlib \
    STRIP=arm-linux-gnueabihf-strip \
        "$sdl_source_dir/configure" \
        --host=arm-linux-gnueabihf \
        --prefix="$sdl_install_dir" \
        --enable-shared \
        --disable-static \
        --disable-audio \
        --disable-video \
        --disable-joystick \
        --disable-haptic \
        --disable-sensor \
        --disable-render \
        --disable-power \
        --disable-filesystem \
        --disable-libudev
    make -j"${BUILD_JOBS:-2}"
    make install
    printf '%s\n' "$sdl_revision" > "$sdl_install_dir/.revision"
fi

codec_revision="$ogg_revision $tremor_revision"
if ! test -f "$codec_install_dir/.revision" || ! grep -Fxq "$codec_revision" "$codec_install_dir/.revision"; then
    ogg_source_dir="$work_dir/ogg-source"
    ogg_build_dir="$work_dir/ogg"
    tremor_source_dir="$work_dir/tremor-source"
    tremor_build_dir="$work_dir/tremor"
    rm -rf "$ogg_source_dir" "$ogg_build_dir" "$tremor_source_dir" "$tremor_build_dir" "$codec_install_dir"
    mkdir -p "$ogg_source_dir" "$ogg_build_dir" "$tremor_source_dir" "$tremor_build_dir" "$codec_install_dir"
    git -C "$repository_root/third-party/ogg" archive "$ogg_revision" | tar -x -C "$ogg_source_dir"
    git -C "$repository_root/third-party/tremor" archive "$tremor_revision" | tar -x -C "$tremor_source_dir"

    mkdir -p "$codec_install_dir/include/ogg" "$codec_install_dir/include/tremor" "$codec_install_dir/lib"
    cat > "$codec_install_dir/include/ogg/config_types.h" <<'EOF'
#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__
#include <stdint.h>
typedef int16_t ogg_int16_t;
typedef uint16_t ogg_uint16_t;
typedef int32_t ogg_int32_t;
typedef uint32_t ogg_uint32_t;
typedef int64_t ogg_int64_t;
typedef uint64_t ogg_uint64_t;
#endif
EOF
    arm-linux-gnueabihf-gcc -O2 -I"$ogg_source_dir/include" -I"$codec_install_dir/include" -c \
        "$ogg_source_dir/src/bitwise.c" -o "$ogg_build_dir/bitwise.o"
    arm-linux-gnueabihf-gcc -O2 -I"$ogg_source_dir/include" -I"$codec_install_dir/include" -c \
        "$ogg_source_dir/src/framing.c" -o "$ogg_build_dir/framing.o"
    arm-linux-gnueabihf-ar rcs "$codec_install_dir/lib/libogg.a" "$ogg_build_dir/bitwise.o" "$ogg_build_dir/framing.o"
    install -m 0644 "$ogg_source_dir/include/ogg/ogg.h" "$ogg_source_dir/include/ogg/os_types.h" \
        "$codec_install_dir/include/ogg/"

    tremor_objects=
    for tremor_source in mdct block window synthesis info floor1 floor0 vorbisfile res012 mapping0 registry codebook sharedbook; do
        arm-linux-gnueabihf-gcc -O2 -I"$tremor_source_dir" -I"$codec_install_dir/include" -c \
            "$tremor_source_dir/$tremor_source.c" -o "$tremor_build_dir/$tremor_source.o"
        tremor_objects="$tremor_objects $tremor_build_dir/$tremor_source.o"
    done
    arm-linux-gnueabihf-ar rcs "$codec_install_dir/lib/libvorbisidec.a" $tremor_objects
    install -m 0644 "$tremor_source_dir/ivorbiscodec.h" "$tremor_source_dir/ivorbisfile.h" \
        "$tremor_source_dir/config_types.h" "$codec_install_dir/include/tremor/"
    printf '%s\n' "$codec_revision" > "$codec_install_dir/.revision"
fi

cd "$source_dir/engine"
cat > version.h <<'EOF'
/* Deterministic BloomOS version metadata for Steward-Fu's 2022 Miyoo port. */
#ifndef VERSION_H
#define VERSION_H
#define VERSION_NAME "OpenBOR"
#define VERSION_MAJOR "4"
#define VERSION_MINOR "0"
#define VERSION_BUILD "0"
#define VERSION_BUILD_INT 0
#define VERSION_COMMIT "b00efbc"
#define VERSION "v" VERSION_MAJOR "." VERSION_MINOR " Build " VERSION_BUILD " (commit hash: " VERSION_COMMIT ")"
#endif
EOF
PKG_CONFIG_PATH="$sdl_install_dir/lib/pkgconfig" \
    make BUILD_MMIYOO=1 \
    CC=arm-linux-gnueabihf-gcc \
    STRIP='arm-linux-gnueabihf-strip OpenBOR.elf -o OpenBOR' \
    LIBRARIES="$sdl_install_dir/lib $codec_install_dir/lib" \
    -j"${BUILD_JOBS:-2}"

test -s OpenBOR
arm-linux-gnueabihf-readelf -h OpenBOR | grep -F 'Machine:' | grep -Fq 'ARM'
arm-linux-gnueabihf-readelf -d OpenBOR | grep -Fq 'Shared library: [libSDL2-2.0.so.0]'
install -m 0755 OpenBOR "$package_dir/OpenBOR"
{
    printf '# OpenBOR and statically linked codec licenses\n\n'
    printf '## OpenBOR\n\n'
    cat "$source_dir/LICENSE"
    printf '\n## libogg\n\n'
    cat "$work_dir/ogg-source/COPYING"
    printf '\n## Tremor\n\n'
    cat "$work_dir/tremor-source/COPYING"
} > "$package_dir/LICENSE.MD"
