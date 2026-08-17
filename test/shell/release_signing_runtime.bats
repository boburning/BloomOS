#!/usr/bin/env bats

@test "release assembly separates the pinned toolchain from OpenSSL 3 signing" {
    grep -F 'unsigned-release: dist package-release-unsigned' /workspace/Makefile
    grep -F 'package-release: package-release-unsigned sign-release' /workspace/Makefile
    grep -F 'openssl pkeyutl -sign' /workspace/Makefile
    grep -F -- '-rawin' /workspace/Makefile
}

@test "every release workflow signs outside the cross-toolchain container" {
    for workflow in \
        /workspace/.github/workflows/hardware-test-build.yml \
        /workspace/.github/workflows/pre-release.yml \
        /workspace/.github/workflows/tagged-release.yml; do
        grep -F 'make unsigned-release' "$workflow"
        grep -F 'make sign-release' "$workflow"
        ! grep -F 'container:' "$workflow"
        build_line="$(grep -n 'make unsigned-release' "$workflow" | cut -d: -f1)"
        secret_line="$(grep -n 'BLOOM_RELEASE_SIGNING_KEY:' "$workflow" | cut -d: -f1)"
        [ "$build_line" -lt "$secret_line" ]
    done
}
