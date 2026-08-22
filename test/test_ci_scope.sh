#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
scope="$root/tools/ci-scope.sh"

expect() {
    expected=$1
    mode=$2
    paths=$3
    actual=$(printf '%s\n' "$paths" | sh "$scope" "$mode")
    if [ "$actual" != "$expected" ]; then
        echo "expected '$expected', got '$actual' for $mode" >&2
        exit 1
    fi
}

expect "bloomShell" build "src/bloomUi/bloom_ui_renderer.c"
expect "gameSwitcher" build "src/gameSwitcher/gameSwitcherAchievements.c"
expect "src/gameSwitcher" static-analysis "src/gameSwitcher/gs_keystate.h"
expect "bloomShell gameSwitcher" build "src/bloomShell/bloom_shell_settings.h"
expect "src/bloomShell src/gameSwitcher" static-analysis "src/bloomShell/bloom_shell_settings.c"
expect "bloomShell gameSwitcher" build "src/bloomShell/bloom_shell_launch.c"
expect "bloomRa bloomShell" build "src/bloomRa/bloom_ra_account.c
src/bloomShell/main.c"
expect "bloomLibrary bloomShell gameSwitcher" build "src/bloomLibrary/bloom_library_query.c"
expect "bloomLaunch bloomShell gameSwitcher" build "src/bloomLaunch/bloom_launch.c"
expect "playActivity gameSwitcher" build "src/playActivity/playActivityModel.c"
expect "bloomSettings bloomShell" build "src/bloomSettings/bloom_settings.c
src/bloomShell/main.c
static/build/.tmp_update/runtime.sh
build/legacy-manifest.json"
expect "src/bloomSettings src/bloomShell" static-analysis "src/bloomSettings/bloom_settings.c
src/bloomShell/main.c
static/build/.tmp_update/runtime.sh
build/legacy-manifest.json"
expect "src/bloomUi src/bloomShell" static-analysis "src/bloomUi/bloom_ui_core.h"
expect "none" static-analysis "test/test_bloom_ui_core.cpp
docs/ARCHITECTURE.md"
expect "bloomShell" build "src/bloomShell/main.c
static/build/.tmp_update/runtime.sh
build/legacy-manifest.json
build/dependencies.lock"
expect "src/bloomShell" static-analysis "src/bloomShell/main.c
static/build/.tmp_update/runtime.sh
build/legacy-manifest.json
build/dependencies.lock"
expect "none" build "static/build/.tmp_update/runtime.sh
build/legacy-manifest.json
build/dependencies.lock"
expect "full" build "src/common/config.mk"
expect "full" build "build/shared-libs/build.sh"
expect "full" static-analysis "Makefile"

grep -F 'git config --global --add safe.directory "$GITHUB_WORKSPACE"' \
    "$root/.github/workflows/build.yml" >/dev/null

echo "ci scope tests passed"
