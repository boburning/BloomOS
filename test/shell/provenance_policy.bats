#!/usr/bin/env bats

setup() {
    POLICY=/workspace/build/provenance-policy.json
}

@test "development artifacts permit explicitly tiered legacy components" {
    run python3 /workspace/tools/provenance_policy.py check-channel \
        --policy "$POLICY" --repository /workspace --channel development

    [ "$status" -eq 0 ]
    [[ "$output" == "provenance policy check-channel: 7 components" ]]
}

@test "stable artifacts fail closed for exact unresolved inventory components" {
    run python3 /workspace/tools/provenance_policy.py check-channel \
        --policy "$POLICY" --repository /workspace --channel stable

    [ "$status" -ne 0 ]
    [[ "$output" == *"inherited-libretro-cores (legacy)"* ]]
    [[ "$output" == *"runtime-tmp-update (unresolved inventory)"* ]]
    [[ "$output" != *"search-filter"* ]]
    [[ "$output" != *"emu-nintendo---ds--drastic"* ]]
    [[ "$output" != *"app-quick-guide"* ]]
}

@test "excluded inventory components cannot enter development artifacts" {
    repository="$BATS_TEST_TMPDIR/repository"
    mkdir -p "$repository/build"
    cp "$POLICY" "$repository/build/provenance-policy.json"
    cp /workspace/build/legacy-manifest.json "$repository/build/legacy-manifest.json"
    python3 - "$repository/build/legacy-manifest.json" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
guide = next(item for item in data["components"] if item["id"] == "app-quick-guide")
guide["resolution"] = "excluded"
with open(path, "w", encoding="utf-8") as stream:
    json.dump(data, stream)
PY

    run python3 /workspace/tools/provenance_policy.py check-channel \
        --policy "$repository/build/provenance-policy.json" \
        --repository "$repository" --channel development

    [ "$status" -ne 0 ]
    [[ "$output" == *"app-quick-guide (excluded inventory)"* ]]
}

@test "inventory policy must cover every manifest component kind" {
    fixture="$BATS_TEST_TMPDIR/policy.json"
    sed 's/"package", "package-common"/"package"/' "$POLICY" >"$fixture"

    run python3 /workspace/tools/provenance_policy.py check-channel \
        --policy "$fixture" --repository /workspace --channel development

    [ "$status" -ne 0 ]
    [[ "$output" == *"component inventory entries are not covered by policy: package-common"* ]]
}

@test "source tier requires source license revision and build recipe" {
    fixture="$BATS_TEST_TMPDIR/policy.json"
    sed '/"license": "MIT",/d' "$POLICY" >"$fixture"

    run python3 /workspace/tools/provenance_policy.py validate \
        --policy "$fixture" --repository /workspace

    [ "$status" -ne 0 ]
    [[ "$output" == *"terminal: license is required for tier source"* ]]
}
