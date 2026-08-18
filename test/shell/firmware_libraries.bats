#!/usr/bin/env bats

@test "runtime loads stock firmware libraries after Bloom overrides" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh

    grep -F 'export LD_LIBRARY_PATH="/lib:/config/lib:$miyoodir/lib:/customer/lib:$sysdir/lib:$sysdir/lib/parasyte"' "$runtime"
    grep -F 'LD_LIBRARY_PATH="$miyoodir/lib:/config/lib:/lib:/customer/lib"' "$runtime"
}

@test "standalone helper environments retain the stock firmware path" {
    run sh -c 'find /workspace/static/build/.tmp_update -type f -name "*.sh" | while IFS= read -r file; do grep -H -E "LD_LIBRARY_PATH=.*miyoodir/lib.*sysdir/lib" "$file"; done | grep -v "/customer/lib"'

    [ "$status" -eq 1 ]
    [ -z "$output" ]
}

@test "miyoo library bundle only carries Bloom gamename and audio preload overrides" {
    run sh -c 'find /workspace/static/build/miyoo/lib -maxdepth 1 -type f -exec basename {} \; | LC_ALL=C sort'

    [ "$status" -eq 0 ]
    [ "$output" = $'libgamename.so\nlibpadsp.so' ]
}
