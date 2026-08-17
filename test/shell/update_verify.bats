#!/usr/bin/env bats

load 'support/test_helper'

setup() {
    setup_bloom_fixture
    export VERIFY=/workspace/static/build/.tmp_update/bin/bloom-update-verify
    export BLOOM_SD_ROOT="$SDCARD"
    export BLOOM_OPENSSL_BIN=/usr/bin/openssl
    export BLOOM_JQ_BIN=/usr/bin/jq
    export BLOOM_UPDATE_PUBLIC_KEY="$BLOOM_TEST_ROOT/public.pem"
    export ARCHIVE="$BLOOM_TEST_ROOT/BloomOS-v1.2.3.zip"
    export MANIFEST="$BLOOM_TEST_ROOT/manifest.json"
    export SIGNATURE="$BLOOM_TEST_ROOT/manifest.sig"
    printf 'release archive\n' >"$ARCHIVE"
    openssl genpkey -algorithm Ed25519 -out "$BLOOM_TEST_ROOT/private.pem" >/dev/null 2>&1
    openssl pkey -in "$BLOOM_TEST_ROOT/private.pem" -pubout -out "$BLOOM_UPDATE_PUBLIC_KEY" >/dev/null 2>&1
    size="$(wc -c <"$ARCHIVE" | tr -d ' ')"
    digest="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
    printf '{"schema":1,"product":"BloomOS","version":"1.2.3","artifacts":[{"filename":"BloomOS-v1.2.3.zip","sha256":"%s","size":%s,"media_type":"application/zip"}]}\n' \
        "$digest" "$size" >"$MANIFEST"
    openssl pkeyutl -sign -inkey "$BLOOM_TEST_ROOT/private.pem" -rawin -in "$MANIFEST" -out "$SIGNATURE"
}

teardown() { teardown_bloom_fixture; }

@test "verifies a signed manifest and its exact archive" {
    run "$VERIFY" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 0 ]
    printf '%s' "$output" | grep -F '"status":"verified"'
    printf '%s' "$output" | grep -F '"version":"1.2.3"'
}

@test "rejects a manifest changed after signing" {
    sed -i 's/1.2.3/9.9.9/' "$MANIFEST"

    run "$VERIFY" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'manifest signature is invalid'
}

@test "rejects an archive that does not match the signed manifest" {
    printf 'tampered\n' >>"$ARCHIVE"

    run "$VERIFY" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'manifest does not match archive'
}

@test "rejects a signature from another release key" {
    openssl genpkey -algorithm Ed25519 -out "$BLOOM_TEST_ROOT/other.pem" >/dev/null 2>&1
    openssl pkeyutl -sign -inkey "$BLOOM_TEST_ROOT/other.pem" -rawin -in "$MANIFEST" -out "$SIGNATURE"

    run "$VERIFY" "$MANIFEST" "$SIGNATURE" "$ARCHIVE"

    [ "$status" -eq 1 ]
    printf '%s' "$output" | grep -F 'manifest signature is invalid'
}
