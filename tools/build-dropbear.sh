#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_REPO="$ROOT_DIR/third-party/dropbear"
OUTPUT="${1:-$ROOT_DIR/build/.tmp_update/bin/dropbear}"

[[ -d "$SOURCE_REPO/.git" || -f "$SOURCE_REPO/.git" ]] || {
    printf 'Dropbear submodule is missing; run git submodule update --init --recursive\n' >&2
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

cat >"$SOURCE_DIR/localoptions.h" <<'EOF'
#define DSS_PRIV_FILENAME "/mnt/SDCARD/.tmp_update/etc/dropbear/dropbear_dss_host_key"
#define RSA_PRIV_FILENAME "/mnt/SDCARD/.tmp_update/etc/dropbear/dropbear_rsa_host_key"
#define ECDSA_PRIV_FILENAME "/mnt/SDCARD/.tmp_update/etc/dropbear/dropbear_ecdsa_host_key"
#define ED25519_PRIV_FILENAME "/mnt/SDCARD/.tmp_update/etc/dropbear/dropbear_ed25519_host_key"
#define INETD_MODE 0
#define DROPBEAR_REEXEC 0
#define DROPBEAR_CLI_LOCALTCPFWD 0
#define DROPBEAR_SVR_LOCALTCPFWD 0
#define DROPBEAR_CLI_REMOTETCPFWD 0
#define DROPBEAR_SVR_REMOTETCPFWD 0
#define DROPBEAR_CLI_AGENTFWD 0
#define DROPBEAR_SVR_AGENTFWD 0
#define DROPBEAR_CLI_PROXYCMD 0
#define DROPBEAR_CLI_NETCAT 0
#define DROPBEAR_USER_ALGO_LIST 0
#define DROPBEAR_AES256 0
#define DROPBEAR_3DES 1
#define DROPBEAR_CHACHA20POLY1305 0
#define DROPBEAR_ENABLE_CBC_MODE 1
#define DROPBEAR_SHA2_256_HMAC 0
#define DROPBEAR_DSS 1
#define DROPBEAR_SK_KEYS 0
#define DROPBEAR_DEFAULT_RSA_SIZE 1024
#define DROPBEAR_DH_GROUP14_SHA256 0
#define DROPBEAR_DH_GROUP1 1
#define DROPBEAR_DH_GROUP1_CLIENTONLY 0
#define DO_MOTD 0
#define DROPBEAR_SVR_PASSWORD_AUTH 1
#define DROPBEAR_SVR_PUBKEY_AUTH 1
#define DROPBEAR_SVR_PUBKEY_OPTIONS 1
#define DROPBEAR_CLI_PUBKEY_AUTH 0
#define DROPBEAR_DEFAULT_CLI_AUTHKEY ""
#define DROPBEAR_USE_PASSWORD_ENV 0
#define DROPBEAR_SFTPSERVER 1
#define SFTPSERVER_PATH "/mnt/SDCARD/.tmp_update/bin/sftp-server"
#define DROPBEAR_PATH_SSH_PROGRAM ""
#define DEFAULT_ROOT_PATH "/customer/app:/mnt/SDCARD/.tmp_update/bin:/usr/sbin:/usr/bin:/sbin:/bin:/config"
#define DEFAULT_LIBRARY_PATH "/lib:/config/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte"
#define DROPBEAR_SVR_NO_SHELL_SESSION 1
#define DROPBEAR_SVR_NO_CHDIR 1
EOF

TOOL_PREFIX="${CROSS_COMPILE:-arm-linux-gnueabihf-}"
if ! command -v "${TOOL_PREFIX}gcc" >/dev/null 2>&1 && \
    [[ -x /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc ]]; then
    TOOL_PREFIX=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-
fi
export CC="${TOOL_PREFIX}gcc"
export STRIP="${TOOL_PREFIX}strip"
export AR="${TOOL_PREFIX}ar"
export RANLIB="${TOOL_PREFIX}ranlib"
export CFLAGS="-Wno-undef -O2 -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve -ffunction-sections -fdata-sections"
export LDFLAGS="-L/usr/local/lib -Wl,--gc-sections -Wl,-rpath=/mnt/SDCARD/.tmp_update/lib"

cd "$SOURCE_DIR"
make clean >/dev/null 2>&1 || true
./configure --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf \
    --disable-harden --disable-zlib --disable-pam --disable-largefile \
    --disable-syslog --enable-bundled-libtom --disable-lastlog \
    --disable-loginfunc --disable-shadow --disable-utmp --disable-utmpx \
    --disable-wtmp --disable-wtmpx --disable-pututline --disable-pututxline >/dev/null
make PROGRAMS="dropbear" -j"$(nproc)" >/dev/null
"$STRIP" dropbear

mkdir -p "$(dirname -- "$OUTPUT")"
cp dropbear "$OUTPUT"
grep -aFq 'authorized_keys' "$OUTPUT"
printf 'Built key-authenticated Dropbear: %s\n' "$OUTPUT"
