#!/usr/bin/env bats

@test "Bloom-owned Tweaks copy contains no accidental Onion branding" {
    run python3 /workspace/tools/validate_branding_copy.py --repository /workspace

    [ "$status" -eq 0 ]
    [[ "$output" == "branding copy validate: 4 classified compatibility literals" ]]
    grep -F 'website hosted by BloomOS' /workspace/src/tweaks/network.h
    grep -F 'between BloomOS and a PC' /workspace/src/tweaks/network.h
    grep -F 'Reset all BloomOS system settings' /workspace/src/tweaks/menus.h
}

@test "branding copy gate rejects a newly introduced Onion product string" {
    repository="$BATS_TEST_TMPDIR/repository"
    mkdir -p "$repository/src/tweaks"
    printf '%s\n' 'const char *label = "Welcome to Onion";' > "$repository/src/tweaks/example.c"

    run python3 /workspace/tools/validate_branding_copy.py --repository "$repository"

    [ "$status" -eq 1 ]
    [[ "$output" == *"unclassified Onion copy: src/tweaks/example.c:1"* ]]
}
