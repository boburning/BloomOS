#!/usr/bin/env bats

@test "legacy manifest exactly matches inherited runtime and package trees" {
    run python3 /workspace/tools/legacy_manifest.py validate \
        --repository /workspace \
        --manifest /workspace/build/legacy-manifest.json

    [ "$status" -eq 0 ]
    [[ "$output" == "legacy manifest validate: 313 components" ]]
    [ ! -e "/workspace/static/packages/RApp/SNK - Neo Geo (GnGeo)" ]
    [ ! -e "/workspace/static/packages/RApp/PICO-8 (PICO-8 standalone)" ]
    [ ! -e "/workspace/static/packages/RApp/SCUMM (ScummVM standalone)" ]
    [ ! -e "/workspace/static/packages/Emu/Nintendo - DS (Drastic)" ]
    [ ! -e "/workspace/static/packages/RApp/PICO-8 (Fake8 standalone)" ]
    [ ! -e "/workspace/static/packages/RApp/Sony - PlayStation (PCSX standalone)" ]
    [ ! -e "/workspace/static/build/.tmp_update/script/drastic_migration.sh" ]
    [ ! -e "/workspace/static/build/.tmp_update/script/migration/00017_drastic_migration.sh" ]
    [ ! -e "/workspace/static/packages/App/Search (Find your games)" ]
    [ ! -e "/workspace/static/packages/App/List shortcuts (Filter+Refresh)" ]
    [ ! -e "/workspace/third-party/SearchFilter" ]
    ! grep -F 'GnGeo' /workspace/.github/create_fullres_files.sh
    ! grep -F 'DraStic' /workspace/.github/create_fullres_files.sh
    ! grep -F 'SearchFilter' /workspace/Makefile
    ! grep -F 'bin/filter' /workspace/static/build/.tmp_update/script/game_list_options.sh
    grep -F './script/reset_list.sh "$romroot"' /workspace/static/build/.tmp_update/script/game_list_options.sh
    [ ! -e /workspace/static/build/miyoo/lib/libgamename.so ]
    grep -F 'cp $(BIN_DIR)/libgamename.so $(BUILD_DIR)/miyoo/lib/' /workspace/Makefile
    [ ! -e /workspace/static/build/.tmp_update/bin/bloom-dropbearmulti ]
    [ ! -e /workspace/static/build/.tmp_update/bin/LICENSE.dropbear ]
    grep -F 'tools/build-dropbear.sh $(BIN_DIR)/bloom-dropbearmulti' /workspace/Makefile
    grep -F 'cp "$SOURCE_REPO/LICENSE" "$(dirname -- "$OUTPUT")/LICENSE.dropbear"' /workspace/tools/build-dropbear.sh
    python3 - /workspace/build/legacy-manifest.json <<'PY'
import json
import sys
components = {item["id"]: item for item in json.load(open(sys.argv[1], encoding="utf-8"))["components"]}
assert "runtime-tmp-update" not in components
assert "runtime-miyoo" not in components
assert "runtime-tmp-update-bin" not in components
assert components["runtime-tmp-update-bin-adv"]["path"] == "static/build/.tmp_update/bin/adv"
assert components["runtime-tmp-update-bin-bloomctl"]["path"] == "static/build/.tmp_update/bin/bloomctl"
assert components["runtime-tmp-update-bin-bloom-ra-login"]["path"] == "static/build/.tmp_update/bin/bloom-ra-login"
assert components["runtime-miyoo-app-skin"]["path"] == "static/build/miyoo/app/skin"
assert components["runtime-miyoo-lib-libpadsp-so"]["path"] == "static/build/miyoo/lib/libpadsp.so"
assert components["runtime-tmp-update-bin-bloomctl"]["resolution"] == "source-build"
assert components["runtime-tmp-update-bin-bloom-power"]["resolution"] == "source-build"
assert components["runtime-tmp-update-script-network"]["path"] == "static/build/.tmp_update/script/network"
assert components["runtime-tmp-update-script-network"]["source"] == "https://github.com/OnionUI/Onion"
assert components["runtime-tmp-update-script-scraper"]["resolution"] == "replace-source-build-or-exclude"
assert components["runtime-tmp-update-lib-libcrypto-so-3"]["path"] == "static/build/.tmp_update/lib/libcrypto.so.3"
assert components["runtime-tmp-update-lib-parasyte"]["path"] == "static/build/.tmp_update/lib/parasyte"
assert components["runtime-tmp-update-keys"]["source"] == "https://github.com/boburning/BloomOS"
assert components["runtime-tmp-update-res-miyoo354-system-json"]["source"] == "https://github.com/OnionUI/Onion"
assert components["runtime-tmp-update-res-wifiup-png"]["resolution"] == "replace-source-build-or-exclude"
PY
}

@test "reviewed runtime source units receive Bloom provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
ids = {
    "runtime-miyoo-app-config-json",
    "runtime-miyoo-app-lang",
    "runtime-miyoo-lib--gitkeep",
    "runtime-tmp-update-config",
    "runtime-tmp-update-etc",
    "runtime-tmp-update-onionversion",
    "runtime-tmp-update-runtime-sh",
    "runtime-tmp-update-updater",
}
for component in data["components"]:
    if component["id"] in ids:
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
    [[ "$output" == "legacy manifest annotated: 8 Bloom source replacements" ]]
    python3 - "$manifest" <<'PY'
import json
import sys
components = json.load(open(sys.argv[1], encoding="utf-8"))["components"]
runtime = [item for item in components if item["id"].startswith("runtime-")]
bloom = [item for item in runtime if item["source"] == "https://github.com/boburning/BloomOS"]
assert len(bloom) == 36
assert all(item["resolution"] == "source-build" for item in bloom)
assert all(item["source_revision"] == "release-commit" for item in bloom)
PY
}

@test "libretro integrations replace the excluded Fake-08 and PCSX standalones" {
    grep -F 'fake08_libretro.so' /workspace/build/core-manifest.json
    grep -F 'pcsx_rearmed_libretro.so' /workspace/build/core-manifest.json
    grep -F 'fake08_libretro.so' "/workspace/static/packages/Emu/PICO-8 (Fake8)/Emu/PICO/launch.sh"
    grep -F 'pcsx_rearmed_libretro.so' "/workspace/static/packages/Emu/Sony - PlayStation (PCSX ReARMed)/Emu/PSX/launch.sh"
}

@test "release builds replace inherited standalones only in package staging" {
    grep -F 'OPENBOR_PACKAGE_DIR="$(PACKAGES_RAPP_DEST)/Game engine - Open Beats of Rage/RApp/OpenBOR"' \
        /workspace/Makefile
    grep -F 'PIXELREADER_PACKAGE_DIR="$(PACKAGES_APP_DEST)/Ebook Reader (PixelReader)/App/PixelReader"' \
        /workspace/Makefile
    grep -F 'OPENBOR_PACKAGE_DIR:-$repository_root/static/packages/' /workspace/build/openbor/build.sh
    grep -F 'PIXELREADER_PACKAGE_DIR:-$root/static/packages/' /workspace/build/pixelreader/build.sh
    grep -F 'work="$root/cache/pixelreader-work"' /workspace/build/pixelreader/build.sh
    ! grep -F 'work="$root/build/pixelreader/work"' /workspace/build/pixelreader/build.sh
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
        static/build/.tmp_update/bin \
        static/build/.tmp_update/lib \
        static/build/.tmp_update/res \
        static/build/.tmp_update/script \
        static/build/miyoo/app \
        static/build/miyoo/lib \
        lib \
        static/packages/common \
        static/packages/App/Demo \
        static/packages/Emu \
        static/packages/RApp; do
        mkdir -p "$repository/$directory"
    done
    printf 'fixture\n' >"$repository/static/build/.tmp_update/bin/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/app/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/lib/runtime.so"
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
    [[ "$output" == *"app-demo: changed file_count, byte_count, tree_sha256"* ]]
}

@test "only UTF-8 Onion script wrappers receive source provenance" {
    repository="$BATS_TEST_TMPDIR/source-repository"
    for directory in \
        static/build/.tmp_update/bin \
        static/build/.tmp_update/lib \
        static/build/.tmp_update/res \
        static/build/.tmp_update/script \
        static/build/miyoo/app \
        static/build/miyoo/lib \
        lib \
        static/packages/common \
        static/packages/App/Source \
        static/packages/App/Opaque \
        static/packages/Emu \
        static/packages/RApp; do
        mkdir -p "$repository/$directory"
    done
    printf 'fixture\n' >"$repository/static/build/.tmp_update/bin/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/app/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/lib/runtime.so"
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
        static/build/.tmp_update/bin \
        static/build/.tmp_update/lib \
        static/build/.tmp_update/res \
        static/build/.tmp_update/script \
        static/build/miyoo/app \
        static/build/miyoo/lib \
        lib \
        static/packages/common \
        "static/packages/App/Battery Monitor/App/BatteryMonitorUI/res" \
        static/packages/Emu \
        static/packages/RApp \
        src/batteryMonitorUI/res; do
        mkdir -p "$repository/$directory"
    done
    printf 'fixture\n' >"$repository/static/build/.tmp_update/bin/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/app/runtime"
    printf 'fixture\n' >"$repository/static/build/miyoo/lib/runtime.so"
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

@test "verified source-built PixelReader receives upstream provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
component = next(item for item in data["components"] if item["id"] == "app-ebook-reader--pixelreader")
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
component = components["app-ebook-reader--pixelreader"]
assert component["resolution"] == "source-build"
assert component["source"] == "https://github.com/ealang/pixel-reader"
assert component["source_revision"] == "762ed8ee40bf24fc05af1b0df1a95d30acd56b5b"
assert component["build_recipe"] == "build/pixelreader/build.sh"
PY
    [ "$status" -eq 0 ]
}

@test "verified source-built shared libraries receive composite provenance" {
    manifest="$BATS_TEST_TMPDIR/legacy-manifest.json"
    cp /workspace/build/legacy-manifest.json "$manifest"
    python3 - "$manifest" <<'PY'
import json
import sys
path = sys.argv[1]
data = json.load(open(path, encoding="utf-8"))
component = next(item for item in data["components"] if item["id"] == "runtime-shared-libraries")
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
component = components["runtime-shared-libraries"]
assert component["resolution"] == "source-build"
assert component["source"] == "https://github.com/boburning/BloomOS"
assert component["license"] == "LicenseRef-SQLite-Public-Domain AND LGPL-2.0-or-later"
assert component["build_recipe"] == "build/shared-libs/build.sh"
PY
    [ "$status" -eq 0 ]
}
