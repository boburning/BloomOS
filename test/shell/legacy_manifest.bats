#!/usr/bin/env bats

@test "legacy manifest exactly matches inherited runtime and package trees" {
    run python3 /workspace/tools/legacy_manifest.py validate \
        --repository /workspace \
        --manifest /workspace/build/legacy-manifest.json

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest validate: 156 components" ]]
}

@test "legacy manifest rejects modified inventory metadata" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    sed 's/"schema": 1/"schema": 2/' \
        /workspace/build/legacy-manifest.json >"$manifest"

    run python3 /workspace/tools/legacy_manifest.py validate \
        --repository /workspace \
        --manifest "$manifest"

    [ "$status" -ne 0 ]
    [[ "$output" == *"unsupported legacy manifest schema"* ]]
}

@test "legacy manifest detects added payload files" {
    repository="$BATS_TEST_TMPDIR/repository"
    for directory in \
        static/build/.tmp_update \
        static/build/miyoo \
        lib \
        static/packages/common \
        static/packages/App/Demo \
        static/packages/Emu \
        static/packages/RApp; do
        mkdir -p "$repository/$directory"
    done
    printf 'fixture\n' >"$repository/static/build/.tmp_update/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/runtime"
    printf 'fixture\n' >"$repository/lib/library.so"
    printf 'fixture\n' >"$repository/static/packages/common/config"
    printf 'fixture\n' >"$repository/static/packages/App/Demo/app"

    python3 /workspace/tools/legacy_manifest.py create \
        --repository "$repository" \
        --manifest "$BATS_TEST_TMPDIR/generated.json"
    printf 'drift\n' >"$repository/static/packages/App/Demo/new-file"

    run python3 /workspace/tools/legacy_manifest.py validate \
        --repository "$repository" \
        --manifest "$BATS_TEST_TMPDIR/generated.json"

    [ "$status" -ne 0 ]
    [[ "$output" == *"stale or non-canonical"* ]]
}

@test "only UTF-8 Onion script wrappers receive source provenance" {
    repository="$BATS_TEST_TMPDIR/source-repository"
    for directory in \
        static/build/.tmp_update \
        static/build/miyoo \
        lib \
        static/packages/common \
        static/packages/App/Source \
        static/packages/App/Opaque \
        static/packages/Emu \
        static/packages/RApp; do
        mkdir -p "$repository/$directory"
    done
    printf 'fixture\n' >"$repository/static/build/.tmp_update/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/runtime"
    printf 'fixture\n' >"$repository/lib/library.so"
    printf 'fixture\n' >"$repository/static/packages/common/config"
    printf '#!/bin/sh\n' >"$repository/static/packages/App/Source/launch.sh"
    printf '{"name":"source"}\n' >"$repository/static/packages/App/Source/config.json"
    printf '\177ELF' >"$repository/static/packages/App/Opaque/program"
    manifest="$BATS_TEST_TMPDIR/source-manifest.json"

    python3 /workspace/tools/legacy_manifest.py create \
        --repository "$repository" --manifest "$manifest"
    run python3 /workspace/tools/legacy_manifest.py annotate-onion-wrappers \
        --repository "$repository" --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 1 Onion source wrappers" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
assert components["app-source"]["resolution"] == "source-build"
assert components["app-source"]["license"] == "GPL-3.0-only"
assert components["app-opaque"]["resolution"] == "replace-source-build-or-exclude"
PY
    [ "$status" -eq 0 ]
}
