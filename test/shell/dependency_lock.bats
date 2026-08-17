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

@test "OpenSSL runtime dependency comes from the pinned ARM toolchain" {
    grep -F 'cp -L /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/lib/libatomic.so.1' /workspace/Makefile
    ! grep -F 'libatomic.so.1' /workspace/Makefile | grep -F 'http'
}
