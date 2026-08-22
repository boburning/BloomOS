#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_REPO="$ROOT_DIR/third-party/openssh-portable"
OUTPUT="${1:-$ROOT_DIR/build/.tmp_update/bin/bloom-sftp-server}"

[[ -d "$SOURCE_REPO/.git" || -f "$SOURCE_REPO/.git" ]] || {
    printf 'OpenSSH portable submodule is missing; run git submodule update --init --recursive\n' >&2
    exit 1
}

SOURCE_DIR="$(mktemp -d)"
cleanup() {
    status=$?
    if [[ $status -ne 0 && -f "$SOURCE_DIR/config.log" ]]; then
        cat "$SOURCE_DIR/config.log" >&2
    fi
    rm -rf "$SOURCE_DIR"
    exit "$status"
}
trap cleanup EXIT
git -C "$SOURCE_REPO" archive HEAD | tar -x -C "$SOURCE_DIR"
touch "$SOURCE_DIR/configure"

TOOL_PREFIX="${CROSS_COMPILE:-arm-linux-gnueabihf-}"
if ! command -v "${TOOL_PREFIX}gcc" >/dev/null 2>&1 && \
    [[ -x /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc ]]; then
    TOOL_PREFIX=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-
fi
export CC="${TOOL_PREFIX}gcc"
export STRIP="${TOOL_PREFIX}strip"
export CFLAGS="-Os -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve -ffunction-sections -fdata-sections"
export LDFLAGS="-Wl,--gc-sections"

cd "$SOURCE_DIR"
./configure --host=arm-linux-gnueabihf --without-openssl --without-zlib \
    --without-pam --disable-strip --prefix=/mnt/SDCARD/.tmp_update >/dev/null
make -s -j"$(nproc)" sftp-server
"$STRIP" sftp-server

mkdir -p "$(dirname -- "$OUTPUT")"
cp sftp-server "$OUTPUT"
cp "$SOURCE_REPO/LICENCE" "$(dirname -- "$OUTPUT")/LICENSE.openssh"
printf 'Built key-authenticated SFTP subsystem: %s\n' "$OUTPUT"
