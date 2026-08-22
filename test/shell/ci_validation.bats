#!/usr/bin/env bats

@test "native workflows are limited to compile-relevant pull request paths" {
    run grep -F "      - 'src/**/*.c'" /workspace/.github/workflows/build.yml
    [ "$status" -eq 0 ]
    run grep -F "      - 'src/**/*.c'" /workspace/.github/workflows/test.yml
    [ "$status" -eq 0 ]
    run grep -F "paths-ignore:" /workspace/.github/workflows/build.yml
    [ "$status" -ne 0 ]
    run grep -F "paths-ignore:" /workspace/.github/workflows/test.yml
    [ "$status" -ne 0 ]
}

@test "native workflows cache pinned submodules and use bounded retries" {
    run grep -F "actions/cache@1bd1e32a3bdc45362d1e726936510720a7c30a57" /workspace/.github/workflows/build.yml
    [ "$status" -eq 0 ]
    run grep -F "tools/checkout-submodules.sh" /workspace/.github/workflows/test.yml
    [ "$status" -eq 0 ]
    run sh -n /workspace/tools/checkout-submodules.sh
    [ "$status" -eq 0 ]
    run grep -F 'safe.directory "$(pwd)"' /workspace/tools/checkout-submodules.sh
    [ "$status" -eq 0 ]
    run grep -F '"$attempt" -ge 4' /workspace/tools/checkout-submodules.sh
    [ "$status" -eq 0 ]
}

@test "hardware builds reuse the pinned recursive submodule cache" {
    workflow=/workspace/.github/workflows/hardware-test-build.yml
    grep -F "actions/cache@1bd1e32a3bdc45362d1e726936510720a7c30a57" "$workflow"
    grep -F 'key: submodules-${{ runner.os }}-${{ steps.submodules.outputs.sha }}' "$workflow"
    grep -F 'run: tools/checkout-submodules.sh' "$workflow"
    ! grep -F 'submodules: recursive' "$workflow"
}

@test "RA release contracts remain part of the complete shell gate" {
    [ ! -e /workspace/.github/workflows/ra-gate.yml ]
    run grep -F 'tools/validate_ra_release_gate.py' /workspace/test/shell/ra_release_gate.bats
    [ "$status" -eq 0 ]
    run grep -F 'run: make test-shell' /workspace/.github/workflows/shell-test.yml
    [ "$status" -eq 0 ]
}
