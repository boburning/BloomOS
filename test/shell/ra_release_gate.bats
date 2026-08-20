#!/usr/bin/env bats

@test "RetroAchievements release-sensitive contracts are complete" {
    run python3 /workspace/tools/validate_ra_release_gate.py --repository /workspace
    [ "$status" -eq 0 ]
    [ "$output" = 'RetroAchievements release gate: ready' ]
}

@test "RetroAchievements release gate rejects permanent proxy config integration" {
    repository="$BATS_TEST_TMPDIR/repository"
    mkdir -p "$repository/build/raofflineproxy" "$repository/build/rhash" "$repository/src" "$repository/test/shell" "$repository/tools"
    cp /workspace/build/ra-core-policy.json /workspace/build/core-manifest.json \
        /workspace/build/dependencies.lock "$repository/build/"
    cp /workspace/build/raofflineproxy/sources.json "$repository/build/raofflineproxy/"
    cp /workspace/build/rhash/* "$repository/build/rhash/"
    cp /workspace/Makefile "$repository/"
    cp -R /workspace/src/bloomLaunch /workspace/src/bloomRaProxy "$repository/src/"
    cp /workspace/tools/validate_ra_core_policy.py /workspace/tools/validate_raofflineproxy_sources.py \
        /workspace/tools/validate_ra_release_gate.py "$repository/tools/"
    for path in test_bloom_launch.cpp test_bloom_ra.cpp test_bloom_ra_account.cpp test_bloom_ra_catalog.cpp \
        test_bloom_ra_database.cpp test_bloom_ra_scanner.cpp; do touch "$repository/test/$path"; done
    for path in ra_cli.bats ra_core_policy.bats ra_proxy_adapter.bats raofflineproxy_sources.bats \
        ra_certification_tool.bats; do touch "$repository/test/shell/$path"; done
    printf '%s\n' 'run_upstream start-proxy' >>"$repository/src/bloomRaProxy/bloom-ra-proxy"
    run python3 "$repository/tools/validate_ra_release_gate.py" --repository "$repository"
    [ "$status" -eq 1 ]
    [[ "$output" == *'forbidden permanent-config integration'* ]]
}
