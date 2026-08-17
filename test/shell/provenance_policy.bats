#!/usr/bin/env bats

setup() {
    POLICY=/workspace/build/provenance-policy.json
}

@test "development artifacts permit explicitly tiered legacy components" {
    run python3 /workspace/tools/provenance_policy.py check-channel \
        --policy "$POLICY" --repository /workspace --channel development

    [ "$status" -eq 0 ]
    [[ "$output" == "provenance policy check-channel: 8 components" ]]
}

@test "stable artifacts fail closed while inherited components remain legacy" {
    run python3 /workspace/tools/provenance_policy.py check-channel \
        --policy "$POLICY" --repository /workspace --channel stable

    [ "$status" -ne 0 ]
    [[ "$output" == *"inherited-libretro-cores (legacy)"* ]]
    [[ "$output" == *"inherited-runtime-payload (legacy)"* ]]
    [[ "$output" == *"inherited-package-catalog (legacy)"* ]]
}

@test "source tier requires source license revision and build recipe" {
    fixture="$BATS_TEST_TMPDIR/policy.json"
    sed '/"license": "MIT",/d' "$POLICY" >"$fixture"

    run python3 /workspace/tools/provenance_policy.py validate \
        --policy "$fixture" --repository /workspace

    [ "$status" -ne 0 ]
    [[ "$output" == *"terminal: license is required for tier source"* ]]
}
