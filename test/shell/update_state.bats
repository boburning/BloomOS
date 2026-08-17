#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export VERIFY=/workspace/static/build/.tmp_update/bin/bloom-update-verify
    export STAGE=/workspace/static/build/.tmp_update/bin/bloom-update-stage
    export PREPARE=/workspace/static/build/.tmp_update/bin/bloom-update-prepare
    export STATE=/workspace/static/build/.tmp_update/bin/bloom-update-state
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_UPDATE_VERIFY_BIN="$VERIFY"
    export BLOOM_OPENSSL_BIN=/usr/bin/openssl
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_7Z_BIN=/usr/bin/7z
    export BLOOM_UPDATE_PUBLIC_KEY="$BLOOM_TEST_ROOT/public.pem"
    export BLOOM_UPDATE_ROOT="$BLOOM_TEST_ROOT/update"
    openssl genpkey -algorithm Ed25519 -out "$BLOOM_TEST_ROOT/private.pem" >/dev/null 2>&1
    openssl pkey -in "$BLOOM_TEST_ROOT/private.pem" -pubout -out "$BLOOM_UPDATE_PUBLIC_KEY" >/dev/null 2>&1
}

teardown() { teardown_bloom_fixture; }

stage_release() {
    version="$1"
    archive="$BLOOM_TEST_ROOT/BloomOS-v$version.zip"
    manifest="$BLOOM_TEST_ROOT/manifest-$version.json"
    signature="$BLOOM_TEST_ROOT/manifest-$version.sig"
    python3 - "$archive" "$version" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1], "w") as archive:
    archive.writestr("miyoo/app/MainUI", "launcher " + sys.argv[2])
    archive.writestr("miyoo/app/.tmp_update/onion.pak", "core")
    archive.writestr("RetroArch/retroarch.pak", "retroarch")
PY
    size="$(wc -c <"$archive" | tr -d ' ')"
    digest="$(sha256sum "$archive" | awk '{print $1}')"
    printf '{"schema":1,"product":"BloomOS","version":"%s","channel":"beta","devices":["mini","plus","flip"],"artifacts":[{"filename":"BloomOS-v%s.zip","sha256":"%s","size":%s,"media_type":"application/zip"}]}\n' \
        "$version" "$version" "$digest" "$size" >"$manifest"
    openssl pkeyutl -sign -inkey "$BLOOM_TEST_ROOT/private.pem" -rawin -in "$manifest" -out "$signature"
    "$STAGE" "$manifest" "$signature" "$archive" >/dev/null
    "$PREPARE" "$version" >/dev/null
}

@test "reports idle before any update is armed" {
    run "$STATE" status

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"phase":"idle"'
}

@test "promotes a verified staged release to known good" {
    stage_release 1.2.3

    run "$STATE" arm 1.2.3
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"phase":"armed"'
    run "$STATE" boot-attempt
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"boot_attempts":1'
    run "$STATE" mark-good 1.2.3
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"phase":"known_good"'
    grep -F '"version":"1.2.3"' "$BLOOM_UPDATE_ROOT/known-good.json"
}

@test "exposes the retained known-good payload after bounded failed boots" {
    stage_release 1.2.3
    "$STATE" arm 1.2.3 >/dev/null
    "$STATE" mark-good 1.2.3 >/dev/null
    stage_release 1.2.4
    "$STATE" arm 1.2.4 >/dev/null

    "$STATE" boot-attempt >/dev/null
    "$STATE" boot-attempt >/dev/null
    run "$STATE" boot-attempt
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"phase":"recovery_required"'
    run "$STATE" recovery
    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"recovery_available"'
    printf '%s' "$output" | grep -F '"version":"1.2.3"'
}

@test "refuses concurrent updates and mismatched promotion" {
    stage_release 1.2.3
    stage_release 1.2.4
    "$STATE" arm 1.2.3 >/dev/null

    run "$STATE" arm 1.2.4
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'another update is active'
    run "$STATE" mark-good 1.2.4
    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'validated version does not match'
}
