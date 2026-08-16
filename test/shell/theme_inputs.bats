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
