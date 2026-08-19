#!/usr/bin/env bats

@test "legacy manifest exactly matches inherited runtime and package trees" {
    run python3 /workspace/tools/legacy_manifest.py validate \
        --repository /workspace \
        --manifest /workspace/build/legacy-manifest.json

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest validate: 154 components" ]]
    [ ! -e "/workspace/static/packages/RApp/SNK - Neo Geo (GnGeo)" ]
    [ ! -e "/workspace/static/packages/RApp/PICO-8 (PICO-8 standalone)" ]
    ! grep -F 'GnGeo' /workspace/.github/create_fullres_files.sh
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
    printf '#!/bin/sh\n' >"$repository/static/packages/common/apply.sh"
    printf '#!/bin/sh\n' >"$repository/static/packages/App/Source/launch.sh"
    printf '{"name":"source"}\n' >"$repository/static/packages/App/Source/config.json"
    printf '/mnt/SDCARD/App/Source/launch.sh\n' >"$repository/static/packages/App/Source/start.miyoocmd"
    printf 'GameName="Source"\n' >"$repository/static/packages/App/Source/source.notfound"
    printf '\177ELF' >"$repository/static/packages/App/Opaque/program"
    manifest="$BATS_TEST_TMPDIR/source-manifest.json"

    python3 /workspace/tools/legacy_manifest.py create \
        --repository "$repository" --manifest "$manifest"
    run python3 /workspace/tools/legacy_manifest.py annotate-onion-wrappers \
        --repository "$repository" --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 2 Onion source wrappers" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
assert components["app-source"]["resolution"] == "source-build"
assert components["app-source"]["license"] == "GPL-3.0-only"
assert components["package-common"]["resolution"] == "source-build"
assert components["package-common"]["license"] == "GPL-3.0-only"
assert components["app-opaque"]["resolution"] == "replace-source-build-or-exclude"
PY
    [ "$status" -eq 0 ]
}

@test "verified Bloom Battery Monitor replacement receives composite provenance" {
    repository="$BATS_TEST_TMPDIR/bloom-repository"
    for directory in \
        static/build/.tmp_update \
        static/build/miyoo \
        lib \
        static/packages/common \
        "static/packages/App/Battery Monitor/App/BatteryMonitorUI/res" \
        static/packages/Emu \
        static/packages/RApp \
        src/batteryMonitorUI/res; do
        mkdir -p "$repository/$directory"
    done
    printf 'fixture\n' >"$repository/static/build/.tmp_update/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/runtime"
    printf 'fixture\n' >"$repository/lib/library.so"
    printf 'fixture\n' >"$repository/static/packages/common/config"
    package="$repository/static/packages/App/Battery Monitor/App/BatteryMonitorUI"
    source="$repository/src/batteryMonitorUI"
    printf '{"name":"Battery Monitor"}\n' >"$package/config.json"
    printf '#!/bin/sh\n' >"$package/launch.sh"
    printf 'include ../common/config.mk\n' >"$source/Makefile"
    cp "$source/Makefile" "$package/Makefile"
    printf 'TTF_OpenFont("./res/DejaVuSans.ttf", 15);\n' >"$source/batteryMonitorUI.c"
    for file in DejaVuSans.ttf DejaVu_LICENSE.txt background.png end.png left_arrow.png right_arrow.png waiting_screen.png; do
        printf 'fixture %s\n' "$file" >"$source/res/$file"
        cp "$source/res/$file" "$package/res/$file"
    done
    manifest="$BATS_TEST_TMPDIR/bloom-manifest.json"

    python3 /workspace/tools/legacy_manifest.py create \
        --repository "$repository" --manifest "$manifest"
    run python3 /workspace/tools/legacy_manifest.py annotate-bloom-replacements \
        --repository "$repository" --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 1 Bloom source replacements" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
battery = components["app-battery-monitor"]
assert battery["resolution"] == "source-build"
assert battery["source"] == "https://github.com/boburning/BloomOS"
assert battery["source_revision"] == "release-commit"
assert battery["license"] == "GPL-3.0-only AND LicenseRef-DejaVu-Fonts"
PY
    [ "$status" -eq 0 ]
}

@test "verified Bloom Quick Guide replacement receives source provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
guide = next(item for item in data["components"] if item["id"] == "app-quick-guide")
for field in ("source", "source_revision", "license", "build_recipe"):
    guide[field] = None
guide["resolution"] = "replace-source-build-or-exclude"
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
PY

    run python3 /workspace/tools/legacy_manifest.py annotate-bloom-replacements \
        --repository /workspace --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 1 Bloom source replacements" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
guide = components["app-quick-guide"]
assert guide["resolution"] == "source-build"
assert guide["source"] == "https://github.com/boburning/BloomOS"
assert guide["source_revision"] == "release-commit"
assert guide["license"] == "GPL-3.0-only"
assert guide["build_recipe"] == "tools/generate_quick_guide.py"
PY
    [ "$status" -eq 0 ]
}

@test "verified Bloom AdvanceMENU wrapper receives source provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
component = next(item for item in data["components"] if item["id"] == "app-advancemenu--alternative-frontend")
for field in ("source", "source_revision", "license", "build_recipe"):
    component[field] = None
component["resolution"] = "replace-source-build-or-exclude"
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
PY

    run python3 /workspace/tools/legacy_manifest.py annotate-bloom-replacements \
        --repository /workspace --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 1 Bloom source replacements" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
component = components["app-advancemenu--alternative-frontend"]
assert component["resolution"] == "source-build"
assert component["source"] == "https://github.com/boburning/BloomOS"
assert component["license"] == "GPL-3.0-only"
assert component["build_recipe"] == "Makefile"
PY
    [ "$status" -eq 0 ]
}

@test "verified source-built Fake-08 receives upstream provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
component = next(item for item in data["components"] if item["id"] == "rapp-pico-8--fake8-standalone")
for field in ("source", "source_revision", "license", "build_recipe"):
    component[field] = None
component["resolution"] = "replace-source-build-or-exclude"
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
PY

    run python3 /workspace/tools/legacy_manifest.py annotate-bloom-replacements \
        --repository /workspace --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 1 Bloom source replacements" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
component = components["rapp-pico-8--fake8-standalone"]
assert component["resolution"] == "source-build"
assert component["source"] == "https://github.com/jtothebell/fake-08"
assert component["source_revision"] == "18a1c8ab686f87f00a418add448ebe872b87869a"
assert component["build_recipe"] == "Makefile"
PY
    [ "$status" -eq 0 ]
}

@test "verified source-built OpenBOR receives upstream provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
component = next(item for item in data["components"] if item["id"] == "rapp-game-engine---open-beats-of-rage")
for field in ("source", "source_revision", "license", "build_recipe"):
    component[field] = None
component["resolution"] = "replace-source-build-or-exclude"
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
PY

    run python3 /workspace/tools/legacy_manifest.py annotate-bloom-replacements \
        --repository /workspace --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 1 Bloom source replacements" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
component = components["rapp-game-engine---open-beats-of-rage"]
assert component["resolution"] == "source-build"
assert component["source"] == "https://github.com/DCurrent/openbor"
assert component["source_revision"] == "b00efbc7752cb55709dfc9fdfdfc7cfe78ddfb90"
assert component["license"] == "BSD-3-Clause"
assert component["build_recipe"] == "build/openbor/build.sh"
PY
    [ "$status" -eq 0 ]
}

@test "verified source-built ScummVM receives upstream provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
component = next(item for item in data["components"] if item["id"] == "rapp-scumm--scummvm-standalone")
for field in ("source", "source_revision", "license", "build_recipe"):
    component[field] = None
component["resolution"] = "replace-source-build-or-exclude"
with open(path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(data, stream, ensure_ascii=False, indent=2)
    stream.write("\n")
PY

    run python3 /workspace/tools/legacy_manifest.py annotate-bloom-replacements \
        --repository /workspace --manifest "$manifest"

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest annotated: 1 Bloom source replacements" ]]
    run python3 - "$manifest" <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
component = components["rapp-scumm--scummvm-standalone"]
assert component["resolution"] == "source-build"
assert component["source"] == "https://github.com/XK9274/scummvm-miyoo"
assert component["source_revision"] == "0a8aa528e92836b86780ba0498dda3263fba19ea"
assert component["build_recipe"] == "build/scummvm/build.sh"
PY
    [ "$status" -eq 0 ]
}
