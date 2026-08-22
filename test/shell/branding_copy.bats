#!/usr/bin/env bats

@test "Bloom-owned stable UI copy contains no accidental Onion branding" {
    run python3 /workspace/tools/validate_branding_copy.py --repository /workspace

    [ "$status" -eq 0 ]
    [[ "$output" == "branding copy validate: 10 classified legacy literals" ]]
    grep -F 'website hosted by BloomOS' /workspace/src/tweaks/network.h
    grep -F 'between BloomOS and a PC' /workspace/src/tweaks/network.h
    grep -F 'Reset all BloomOS system settings' /workspace/src/tweaks/menus.h
    grep -F 'BloomOS save folder map' /workspace/static/configs/Saves/CurrentProfile/saves/README.txt
    grep -F 'label = BloomOS-v{VERSION}' /workspace/static/build/autorun.inf
    grep -F '"BloomOS "' /workspace/src/easter/easter.c
    grep -F 'UPSTREAM ONIONUI CORE TEAM' /workspace/static/build/.tmp_update/onionVersion/acknowledgments.txt
    ! grep -R 'ONION_VERSION' /workspace/src/common/config.mk /workspace/src/installUI/installUI.c
}

@test "branding copy gate rejects a newly introduced Onion product string" {
    repository="$BATS_TEST_TMPDIR/repository"
    mkdir -p "$repository/src/tweaks"
    printf '%s\n' 'const char *label = "Welcome to Onion";' > "$repository/src/tweaks/example.c"

    run python3 /workspace/tools/validate_branding_copy.py --repository "$repository"

    [ "$status" -eq 1 ]
    [[ "$output" == *"unclassified Onion copy: src/tweaks/example.c:1"* ]]
}

@test "branding copy gate covers stable Bloom Shell sources" {
    repository="$BATS_TEST_TMPDIR/repository"
    mkdir -p "$repository"
    cp -R /workspace/src "$repository/src"
    mkdir -p "$repository/static/configs/Saves/CurrentProfile/states"
    mkdir -p "$repository/static/configs/Saves/CurrentProfile/saves"
    mkdir -p "$repository/static/build/.tmp_update/onionVersion"
    mkdir -p "$repository/static/packages/App/Quick Guide/App/Onion_Manual"
    cp -R /workspace/static/configs/Saves/CurrentProfile/states/README.txt "$repository/static/configs/Saves/CurrentProfile/states/"
    cp -R /workspace/static/configs/Saves/CurrentProfile/saves/README.txt "$repository/static/configs/Saves/CurrentProfile/saves/"
    cp /workspace/static/build/autorun.inf "$repository/static/build/"
    cp /workspace/static/build/.tmp_update/onionVersion/acknowledgments.txt "$repository/static/build/.tmp_update/onionVersion/"
    cp -R "/workspace/static/packages/App/Quick Guide/App/Onion_Manual/." "$repository/static/packages/App/Quick Guide/App/Onion_Manual/"
    printf '%s\n' 'const char *label = "Return to Onion";' > "$repository/src/bloomShell/accidental_brand.c"

    run python3 /workspace/tools/validate_branding_copy.py --repository "$repository"

    [ "$status" -eq 1 ]
    [[ "$output" == *"unclassified Onion copy: src/bloomShell/accidental_brand.c:1"* ]]
}
