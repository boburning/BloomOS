#!/usr/bin/env bats

@test "Bloom branding assets reproduce from repository-owned source" {
    run python3 /workspace/tools/generate_bloom_brand_assets.py --repository /workspace --check

    [ "$status" -eq 0 ]
    [ "$output" = "Bloom branding assets: reproducible" ]
    cmp /workspace/assets/branding/bloom-boot-640x480.png /workspace/src/bootScreen/res/bootScreen.png
}

@test "Bloom branding generation rejects a divergent canonical mark" {
    repository="$BATS_TEST_TMPDIR/repository"
    mkdir -p "$repository/build" "$repository/src/bloomUi/assets"
    cp /workspace/build/bloom-design-tokens.json "$repository/build/"
    printf '%s\n' '<svg><path fill="#000000"/></svg>' > "$repository/src/bloomUi/assets/bloom-mark.svg"

    run python3 /workspace/tools/generate_bloom_brand_assets.py --repository "$repository" --check

    [ "$status" -eq 1 ]
    [[ "$output" == *"canonical Bloom mark is incompatible"* ]]
}
