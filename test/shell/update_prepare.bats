#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export VERIFY=/workspace/static/build/.tmp_update/bin/bloom-update-verify
    export STAGE=/workspace/static/build/.tmp_update/bin/bloom-update-stage
    export PREPARE=/workspace/static/build/.tmp_update/bin/bloom-update-prepare
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_UPDATE_VERIFY_BIN="$VERIFY"
    export BLOOM_OPENSSL_BIN=/usr/bin/openssl
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_7Z_BIN=/usr/bin/7z
    export BLOOM_UPDATE_PUBLIC_KEY="$BLOOM_TEST_ROOT/public.pem"
    export BLOOM_UPDATE_ROOT="$BLOOM_TEST_ROOT/update"
    export ARCHIVE="$BLOOM_TEST_ROOT/BloomOS-v1.2.3.zip"
    export MANIFEST="$BLOOM_TEST_ROOT/manifest.json"
    export SIGNATURE="$BLOOM_TEST_ROOT/manifest.sig"
    openssl genpkey -algorithm Ed25519 -out "$BLOOM_TEST_ROOT/private.pem" >/dev/null 2>&1
    openssl pkey -in "$BLOOM_TEST_ROOT/private.pem" -pubout -out "$BLOOM_UPDATE_PUBLIC_KEY" >/dev/null 2>&1
}

teardown() { teardown_bloom_fixture; }

sign_and_stage() {
    size="$(wc -c <"$ARCHIVE" | tr -d ' ')"
    digest="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
    printf '{"schema":1,"product":"BloomOS","version":"1.2.3","channel":"beta","devices":["mini","plus","flip"],"artifacts":[{"filename":"BloomOS-v1.2.3.zip","sha256":"%s","size":%s,"media_type":"application/zip"}]}\n' \
        "$digest" "$size" >"$MANIFEST"
    openssl pkeyutl -sign -inkey "$BLOOM_TEST_ROOT/private.pem" -rawin -in "$MANIFEST" -out "$SIGNATURE"
    "$STAGE" "$MANIFEST" "$SIGNATURE" "$ARCHIVE" >/dev/null
}

@test "extracts a reverified release only into an isolated candidate" {
    python3 - "$ARCHIVE" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as archive:
    archive.writestr("miyoo/app/MainUI", "launcher")
    archive.writestr("miyoo/app/.tmp_update/onion.pak", "core")
    archive.writestr("RetroArch/retroarch.pak", "retroarch")
PY
    sign_and_stage

    run "$PREPARE" 1.2.3

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"prepared"'
    candidate="$BLOOM_UPDATE_ROOT/candidates/1.2.3"
    [ -f "$candidate/miyoo/app/MainUI" ]
    [ -f "$candidate/miyoo/app/.tmp_update/onion.pak" ]
    [ -f "$candidate/RetroArch/retroarch.pak" ]
    [ -f "$candidate/verified.json" ]
    [ ! -e "$SDCARD/miyoo/app/MainUI" ]
}

@test "replace rebuilds a cached candidate from the signed archive" {
    python3 - "$ARCHIVE" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as archive:
    archive.writestr("miyoo/app/MainUI", "signed launcher")
    archive.writestr("miyoo/app/.tmp_update/onion.pak", "signed core")
    archive.writestr("RetroArch/retroarch.pak", "signed retroarch")
PY
    sign_and_stage
    mkdir -p "$BLOOM_UPDATE_ROOT/candidates/1.2.3/miyoo/app/.tmp_update"
    printf 'tampered\n' >"$BLOOM_UPDATE_ROOT/candidates/1.2.3/miyoo/app/.tmp_update/onion.pak"

    run "$PREPARE" --replace 1.2.3

    [ "$status" -eq 0 ]
    grep -Fx 'signed core' "$BLOOM_UPDATE_ROOT/candidates/1.2.3/miyoo/app/.tmp_update/onion.pak"
    ! find "$BLOOM_UPDATE_ROOT/candidates" -maxdepth 1 -name '.replaced-*' | grep -q .
}

@test "rejects a signed archive with path traversal" {
    python3 - "$ARCHIVE" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as archive:
    archive.writestr("miyoo/app/MainUI", "launcher")
    archive.writestr("miyoo/app/.tmp_update/onion.pak", "core")
    archive.writestr("RetroArch/retroarch.pak", "retroarch")
    archive.writestr("miyoo/../../escape", "unsafe")
PY
    sign_and_stage

    run "$PREPARE" 1.2.3

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'unsafe path'
    [ ! -e "$BLOOM_UPDATE_ROOT/candidates/1.2.3" ]
    [ ! -e "$BLOOM_TEST_ROOT/escape" ]
}

@test "rejects unexpected top-level content" {
    python3 - "$ARCHIVE" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as archive:
    archive.writestr("miyoo/app/MainUI", "launcher")
    archive.writestr("miyoo/app/.tmp_update/onion.pak", "core")
    archive.writestr("RetroArch/retroarch.pak", "retroarch")
    archive.writestr("autorun.sh", "unsafe")
PY
    sign_and_stage

    run "$PREPARE" 1.2.3

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'unexpected top-level path'
}
