#!/usr/bin/env bats

@test "theme build inputs are commit-pinned and checksum-locked" {
    run grep -E '^readonly THEMES_REVISION="[0-9a-f]{40}"$' /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]

    run grep -E 'raw\.githubusercontent\.com/OnionUI/Themes/\$THEMES_REVISION/release' \
        /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]

    run grep -E '(^|/)main/' /workspace/.github/get_themes.sh
    [ "$status" -ne 0 ]

    entry_count="$(grep -Ec '^[0-9a-f]{64}  .+\.zip$' /workspace/build/themes.sha256)"
    [ "$entry_count" -eq 20 ]
    [ "$entry_count" -eq "$(grep -E '^[0-9a-f]{64}  .+\.zip$' /workspace/build/themes.sha256 | cut -d' ' -f3- | sort -u | wc -l)" ]
}

@test "theme downloader verifies cached and downloaded artifacts" {
    run grep -F 'sha256sum --check --status' /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]
    [ "${#lines[@]}" -eq 2 ]

    run grep -F 'rm -f "$zipfile"' /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]
}

@test "theme downloader bounds retries and keeps partial files out of the cache" {
    run grep -F 'readonly DOWNLOAD_ATTEMPTS=8' /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]

    run grep -F -- '--tries=1 --timeout="$DOWNLOAD_TIMEOUT"' /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]

    run grep -F 'download_tmp="${destination}.part.$$"' /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]

    run grep -F 'mv "$download_tmp" "$destination"' /workspace/.github/get_themes.sh
    [ "$status" -eq 0 ]
}

@test "theme downloader retries transient failures before publishing the artifact" {
    repo="$BATS_TEST_TMPDIR/repo"
    fakebin="$BATS_TEST_TMPDIR/bin"
    mkdir -p "$repo/.github" "$repo/build" "$fakebin"
    cp /workspace/.github/get_themes.sh "$repo/.github/get_themes.sh"
    printf 'complete theme archive\n' >"$BATS_TEST_TMPDIR/payload"
    hash="$(sha256sum "$BATS_TEST_TMPDIR/payload" | cut -d' ' -f1)"
    printf '%s  Test.zip\n' "$hash" >"$repo/build/themes.sha256"

    cat >"$fakebin/wget" <<'EOF'
#!/bin/sh
count="$(cat "$MOCK_COUNT" 2>/dev/null || printf 0)"
count=$((count + 1))
printf '%s\n' "$count" >"$MOCK_COUNT"
while [ "$#" -gt 0 ]; do
    if [ "$1" = "-O" ]; then
        output="$2"
        break
    fi
    shift
done
if [ "$count" -lt 3 ]; then
    printf 'partial\n' >"$output"
    exit 8
fi
cp "$MOCK_PAYLOAD" "$output"
EOF
    cat >"$fakebin/sleep" <<'EOF'
#!/bin/sh
exit 0
EOF
    chmod +x "$fakebin/wget" "$fakebin/sleep"

    export MOCK_COUNT="$BATS_TEST_TMPDIR/count"
    export MOCK_PAYLOAD="$BATS_TEST_TMPDIR/payload"
    run env PATH="$fakebin:$PATH" bash "$repo/.github/get_themes.sh"
    [ "$status" -eq 0 ]
    [ "$(cat "$MOCK_COUNT")" -eq 3 ]
    artifact="$repo/cache/themes/b01198352e8927c3c5b9a828f73177bc81745954/Test.zip"
    run sh -c "printf '%s  %s\\n' '$hash' '$artifact' | sha256sum --check --status"
    [ "$status" -eq 0 ]
    run find "$repo/cache" -name '*.part.*' -print
    [ "$status" -eq 0 ]
    [ -z "$output" ]
    [ -f "$artifact" ]
}
