#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work_dir="${PCSX_BUILD_DIR:-$repository_root/cache/pcsx-build}"
source_dir="$work_dir/source"
sdl_source_dir="$work_dir/sdl-source"
package_dir="$repository_root/static/packages/RApp/Sony - PlayStation (PCSX standalone)/RApp/PCSX-ReARMed"

pcsx_revision=8987ee208f057b59a35815f4e6a805935faf2fc8
libpicofe_revision=7167e5f3376f0d0692ae102ed2df1ef5d2cc199a
warm_revision=a6f015da3b10b82a476250793645c071340decbc
libchdr_revision=a03e69319164f69d781ab8e453f8cf407387bd13
sdl_revision=0e0919585f2f809471ba45bdc16624ef4e887bc0

test "$(git -C "$repository_root/third-party/pcsx_rearmed" rev-parse HEAD)" = "$pcsx_revision"
test "$(git -C "$repository_root/third-party/pcsx_rearmed/frontend/libpicofe" rev-parse HEAD)" = "$libpicofe_revision"
test "$(git -C "$repository_root/third-party/pcsx_rearmed/frontend/warm" rev-parse HEAD)" = "$warm_revision"
test "$(git -C "$repository_root/third-party/pcsx_rearmed/libchdr" rev-parse HEAD)" = "$libchdr_revision"
test "$(git -C "$repository_root/third-party/SDL-1.2" rev-parse HEAD)" = "$sdl_revision"

rm -rf "$source_dir" "$sdl_source_dir"
mkdir -p "$source_dir" "$sdl_source_dir"
git -C "$repository_root/third-party/pcsx_rearmed" archive "$pcsx_revision" | tar -x -C "$source_dir"
for nested_source in frontend/libpicofe frontend/warm libchdr; do
    mkdir -p "$source_dir/$nested_source"
done
git -C "$repository_root/third-party/pcsx_rearmed/frontend/libpicofe" archive "$libpicofe_revision" | tar -x -C "$source_dir/frontend/libpicofe"
git -C "$repository_root/third-party/pcsx_rearmed/frontend/warm" archive "$warm_revision" | tar -x -C "$source_dir/frontend/warm"
git -C "$repository_root/third-party/pcsx_rearmed/libchdr" archive "$libchdr_revision" | tar -x -C "$source_dir/libchdr"
git -C "$repository_root/third-party/SDL-1.2" archive "$sdl_revision" | tar -x -C "$sdl_source_dir"

cd "$sdl_source_dir"
# Upstream carries CRLF in parts of its build machinery. Normalize the scripts
# and the one patched source file so execution and patching are host-neutral.
sed -i 's/\r$//' config.sh configure build-scripts/* src/audio/miao/SDL_miaoaudio.c
patch -p1 < "$repository_root/build/pcsx/sdl-no-msettings.patch"
export SOURCE_DATE_EPOCH=1761113005
CROSS_COMPILE=arm-linux-gnueabihf- ./config.sh
make -j"${BUILD_JOBS:-2}"
test -s build/.libs/libSDL-1.2.so.0.11.5
arm-linux-gnueabihf-readelf -d build/.libs/libSDL-1.2.so.0.11.5 | grep -Fq 'Shared library: [libmi_gfx.so]'
if arm-linux-gnueabihf-readelf -d build/.libs/libSDL-1.2.so.0.11.5 | grep -Fq 'libmsettings'; then
    echo 'PCSX SDL unexpectedly depends on libmsettings' >&2
    exit 1
fi
arm-linux-gnueabihf-strip build/.libs/libSDL-1.2.so.0.11.5

cd "$source_dir"
export SOURCE_DATE_EPOCH=1670976101
CROSS_COMPILE=arm-linux-gnueabihf- \
CFLAGS='-O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard' \
    ./configure
# The external GPU plugin recipes share plugins/gpulib build objects and race
# under parallel make, producing nondeterministic plugin binaries.
make -j1

test -s pcsx
for plugin in gpu_peops gpu_senquack gpu_unai spunull; do
    test -s "plugins/$plugin.so"
done
# Remove path-bearing debug data and LTO-generated local symbols. The runtime
# code is stable, but those sections vary between otherwise identical builds.
arm-linux-gnueabihf-strip pcsx
for plugin in gpu_peops gpu_senquack gpu_unai spunull; do
    arm-linux-gnueabihf-strip "plugins/$plugin.so"
done
arm-linux-gnueabihf-readelf -h pcsx | grep -F 'Machine:' | grep -Fq 'ARM'
arm-linux-gnueabihf-readelf -A pcsx | grep -Fq 'Tag_CPU_arch: v7'
arm-linux-gnueabihf-readelf -A pcsx | grep -Fq 'Tag_Advanced_SIMD_arch: NEONv1'
arm-linux-gnueabihf-readelf -d pcsx | grep -Fq 'Shared library: [libSDL-1.2.so.0]'

install -m 0755 pcsx "$package_dir/pcsx"
install -m 0755 "$sdl_source_dir/build/.libs/libSDL-1.2.so.0.11.5" "$package_dir/lib/libSDL-1.2.so.0"
for plugin in gpu_peops gpu_senquack gpu_unai spunull; do
    install -m 0755 "plugins/$plugin.so" "$package_dir/plugins/$plugin.so"
done
rm -f "$package_dir/cheatpops.db"
rm -rf "$package_dir/.pcsx/memcards" "$package_dir/skin"
mkdir -p "$package_dir/skin"
cp -a frontend/320240/skin/. "$package_dir/skin/"
{
    printf '# PCSX-ReARMed standalone licenses\n\n'
    printf '## PCSX-ReARMed\n\n'
    sed 's/[[:space:]]*$//' COPYING
    printf '\n## libchdr\n\n'
    sed 's/[[:space:]]*$//' libchdr/LICENSE.txt
    printf '\n## LZMA SDK\n\n'
    sed 's/[[:space:]]*$//' libchdr/deps/lzma-19.00/LICENSE
    printf '\n## SDL 1.2\n\n'
    sed 's/[[:space:]]*$//' "$sdl_source_dir/COPYING"
    printf '\n## BloomOS and Onion package wrappers\n\n'
    sed 's/[[:space:]]*$//' "$repository_root/LICENSE"
} > "$package_dir/LICENSE.MD"
