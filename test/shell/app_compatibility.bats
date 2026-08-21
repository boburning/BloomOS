#!/usr/bin/env bats

@test "only reviewed applications declare Bloom compatibility" {
    run sh -c '
        find /workspace/static/packages/App -name config.json -type f -print0 |
        sort -z |
        xargs -0 /usr/bin/jq -r '\''select(has("bloom_compatibility")) | [.label,.bloom_compatibility] | @tsv'\''
    '

    [ "$status" -eq 0 ]
    [ "$output" = "Activity Tracker	bloom-native
Terminal	development-only" ]
}
