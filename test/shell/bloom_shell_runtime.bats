#!/usr/bin/env bats

@test "development Bloom Shell has an explicit MainUI fallback and structured launch exit" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    grep -F 'elif [ -f "$sysdir/config/.bloomShell" ]; then' "$runtime"
    grep -F '"$sysdir/bin/bloom-shell" >/dev/null 2>&1' "$runtime"
    grep -F '[ "$shell_status" -eq 20 ] && [ -f "$sysdir/cmd_to_run.sh" ]' "$runtime"
    grep -F 'launch_main_ui' "$runtime"
}

@test "Bloom Shell render loop does not spawn command-line consumers" {
    source=/workspace/src/bloomShell/main.c
    ! grep -E '\b(system|popen|fork|exec[a-z]*)[[:space:]]*\(' "$source"
    grep -F 'bloom_library_query_games(database, "gb"' "$source"
    grep -F 'bloom_library_query_recents(database, "gb"' "$source"
    grep -F 'bloom_shell_stage_game(&games[focus.selected]' "$source"
    grep -F 'bloom_shell_stage_game(&recent, GB_CORE' "$source"
}
