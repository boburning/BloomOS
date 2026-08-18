#!/usr/bin/env bats

@test "build lock records the cross-toolchain components" {
    lock=/workspace/build/dependencies.lock
    grep -F 'gcc = "GNU Toolchain for the A-profile Architecture 8.3-2019.03 (arm-rel-8.36), GCC 8.3.0"' "$lock"
    grep -F 'binutils_ld = "GNU ld 2.32.0.20190321"' "$lock"
    grep -F 'python = "3.7.3"' "$lock"
    grep -F 'p7zip = "16.02"' "$lock"
    grep -F 'info_zip = "3.0"' "$lock"
}

@test "build lock records every recursive source submodule" {
    lock=/workspace/build/dependencies.lock
    while read -r revision path _description; do
        revision="${revision#-}"
        revision="${revision#+}"
        grep -F "\"$path\" = \"$revision\"" "$lock"
    done < <(git -C /workspace submodule status --recursive)
}

@test "resolved toolchain and RetroArch fields are not left unresolved" {
    lock=/workspace/build/dependencies.lock
    run grep -E '^(gcc|gxx|binutils_ld|binutils_as|source_revision|patch_revision) = "UNRESOLVED"$' "$lock"
    [ "$status" -ne 0 ]
}

@test "Battery Monitor font is checksum-pinned with its license" {
    lock=/workspace/build/dependencies.lock

    grep -F 'version = "2.37"' "$lock"
    grep -F 'archive_sha256 = "7576310b219e04159d35ff61dd4a4ec4cdba4f35c00e002a136f00e96a908b0a"' "$lock"
    grep -F 'font_sha256 = "7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954"' "$lock"
    grep -F 'license_file = "src/batteryMonitorUI/res/DejaVu_LICENSE.txt"' "$lock"
    [ "$(sha256sum /workspace/src/batteryMonitorUI/res/DejaVuSans.ttf | cut -d' ' -f1)" = \
        "7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954" ]
    cmp /workspace/src/batteryMonitorUI/res/DejaVuSans.ttf \
        "/workspace/static/packages/App/Battery Monitor/App/BatteryMonitorUI/res/DejaVuSans.ttf"
    cmp /workspace/src/batteryMonitorUI/res/DejaVu_LICENSE.txt \
        "/workspace/static/packages/App/Battery Monitor/App/BatteryMonitorUI/res/DejaVu_LICENSE.txt"
}

@test "Fake-08 standalone is source-pinned and license-complete" {
    lock=/workspace/build/dependencies.lock
    package="/workspace/static/packages/RApp/PICO-8 (Fake8 standalone)/RApp/PICO"

    grep -F 'version = "0.0.2.19"' "$lock"
    grep -F 'source_revision = "18a1c8ab686f87f00a418add448ebe872b87869a"' "$lock"
    grep -F 'binary_sha256 = "e3d5f948413d181b98ae8b0883a72938fac38ea9013ae587fffee384a1a0f9b4"' "$lock"
    [ "$(sha256sum "$package/FAKE08" | cut -d' ' -f1)" = \
        "e3d5f948413d181b98ae8b0883a72938fac38ea9013ae587fffee384a1a0f9b4" ]
    cmp /workspace/third-party/fake-08/LICENSE.MD "$package/LICENSE.MD"
    grep -F 'cd $(THIRD_PARTY_DIR)/fake-08 && make miyoomini' /workspace/Makefile
}

@test "OpenBOR standalone is source-pinned and license-complete" {
    lock=/workspace/build/dependencies.lock
    package="/workspace/static/packages/RApp/Game engine - Open Beats of Rage/RApp/OpenBOR"

    grep -F 'source_revision = "b00efbc7752cb55709dfc9fdfdfc7cfe78ddfb90"' "$lock"
    grep -F 'sdl_link_revision = "b424665e0899769b200231ba943353a5fee1b6b6"' "$lock"
    grep -F 'ogg_revision = "e1774cd77f471443541596e09078e78fdc342e4f"' "$lock"
    grep -F 'tremor_revision = "820fb3237ea81af44c9cc468c8b4e20128e3e5ad"' "$lock"
    grep -F 'binary_sha256 = "41ef99389f37a943eb67b9f50cb2847ff00c646f0f3d63598611684053b19c57"' "$lock"
    [ "$(sha256sum "$package/OpenBOR" | cut -d' ' -f1)" = \
        "41ef99389f37a943eb67b9f50cb2847ff00c646f0f3d63598611684053b19c57" ]
    grep -F '## OpenBOR' "$package/LICENSE.MD"
    grep -F '## libogg' "$package/LICENSE.MD"
    grep -F '## Tremor' "$package/LICENSE.MD"
    grep -F './build/openbor/build.sh' /workspace/Makefile
}

@test "PCSX standalone and private SDL are source-pinned and reproducible" {
    lock=/workspace/build/dependencies.lock
    package="/workspace/static/packages/RApp/Sony - PlayStation (PCSX standalone)/RApp/PCSX-ReARMed"

    grep -F 'source_revision = "8987ee208f057b59a35815f4e6a805935faf2fc8"' "$lock"
    grep -F 'sdl_revision = "0e0919585f2f809471ba45bdc16624ef4e887bc0"' "$lock"
    grep -F 'binary_sha256 = "ab0a2048b2b54aa357cc172e94ef6a611a35c571853155bcef5f27d962f66da4"' "$lock"
    grep -F 'sdl_sha256 = "8fef9e7d38dfce315fd1c9c7dee78a6926228eaf4dbbe42b26e069c8f511b4a5"' "$lock"
    [ "$(sha256sum "$package/pcsx" | cut -d' ' -f1)" = \
        "ab0a2048b2b54aa357cc172e94ef6a611a35c571853155bcef5f27d962f66da4" ]
    [ "$(sha256sum "$package/lib/libSDL-1.2.so.0" | cut -d' ' -f1)" = \
        "8fef9e7d38dfce315fd1c9c7dee78a6926228eaf4dbbe42b26e069c8f511b4a5" ]
    grep -F '## PCSX-ReARMed' "$package/LICENSE.MD"
    grep -F '## SDL 1.2' "$package/LICENSE.MD"
    grep -F '## BloomOS and Onion package wrappers' "$package/LICENSE.MD"
    cmp -s /workspace/third-party/pcsx_rearmed/frontend/320240/skin/background.png \
        "$package/skin/background.png"
    [ ! -e "$package/pcsx-fromMiyoo" ]
    [ ! -e "$package/cheatpops.db" ]
    [ ! -e "$package/.pcsx/memcards" ]
    grep -F './build/pcsx/build.sh' /workspace/Makefile
}

@test "OpenSSL runtime dependency comes from the pinned ARM toolchain" {
    grep -F 'cp -L /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/lib/libatomic.so.1' /workspace/Makefile
    ! grep -F 'libatomic.so.1' /workspace/Makefile | grep -F 'http'
}

@test "release tooling does not resolve a moving upstream latest release" {
    run grep -R -E 'api\.github\.com/.*/releases/latest|Onion-latest\.zip|create_patch\.sh' \
        /workspace/Makefile /workspace/.github /workspace/tools
    [ "$status" -ne 0 ]
}

@test "legacy inventory totals match the dependency lock" {
    python3 - <<'PY'
import json
import pathlib
import re

root = pathlib.Path('/workspace')
manifest = json.loads((root / 'build/legacy-manifest.json').read_text(encoding='utf-8'))
lock = (root / 'build/dependencies.lock').read_text(encoding='utf-8')
components = manifest['components']
expected = {
    'component_count': len(components),
    'file_count': sum(item['file_count'] for item in components),
    'byte_count': sum(item['byte_count'] for item in components),
    'source_build_components': sum(item['resolution'] == 'source-build' for item in components),
    'unresolved_components': sum(item['resolution'] == 'replace-source-build-or-exclude' for item in components),
}
for key, value in expected.items():
    match = re.search(rf'^{key} = (\d+)$', lock, re.MULTILINE)
    assert match and int(match.group(1)) == value, (key, value)
PY
}
