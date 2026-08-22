#!/usr/bin/env bats

@test "only reviewed classifications are declared for carried applications" {
    run sh -c '
        find /workspace/static/packages/App -name config.json -type f -print0 |
        sort -z |
        xargs -0 /usr/bin/jq -r '\''select(has("bloom_compatibility")) | [.label,.bloom_compatibility] | @tsv'\''
    '

    [ "$status" -eq 0 ]
    [ "$output" = "Activity Tracker	mainui-dependent
Quick Guide	bloom-native
Terminal	development-only" ]
}

@test "stable package staging excludes migrated settings and developer utilities" {
    makefile=/workspace/Makefile

    # Activity Tracker remains packaged so updates can revoke its former
    # bloom-native declaration without deleting rollback-compatible files.
    grep -F '$(PACKAGES_APP_DEST)/Activity Tracker/.' "$makefile"
    grep -F '$(PACKAGES_APP_DEST)/Quick Guide/.' "$makefile"
    ! grep -F '$(PACKAGES_APP_DEST)/Tweaks/.' "$makefile"
    ! grep -F '$(PACKAGES_APP_DEST)/ThemeSwitcher/.' "$makefile"
    ! grep -F '$(PACKAGES_APP_DEST)/RetroArch (Shortcut)/.' "$makefile"
}

@test "crashing legacy Activity Tracker is not exposed as a stable Bloom app" {
    config="/workspace/static/packages/App/Activity Tracker/App/PlayActivity/config.json"

    [ "$(jq -r '.bloom_compatibility' "$config")" = mainui-dependent ]
    grep -F '| Activity Tracker | `MIGRATE_TO_CONTEXT_ACTION` | No |' \
        /workspace/docs/APP_DISPOSITIONS_1.0.md
}

@test "Apps hides unreviewed and migrated utilities by compatibility policy" {
    query=/workspace/src/bloomLibrary/bloom_library_query.c

    grep -F "compatibility IN('bloom-native','onion-compatible')" "$query"
    grep -F "compatibility='development-only'" "$query"
    grep -F 'include_development' "$query"
}

@test "every carried application package has a 1.0 product disposition" {
    dispositions=/workspace/docs/APP_DISPOSITIONS_1.0.md

    while IFS= read -r package; do
        grep -F "| $package |" "$dispositions"
    done < <(find /workspace/static/packages/App -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort)
    grep -F '| Package Manager | `MIGRATE_TO_DEVELOPER` |' "$dispositions"
}
