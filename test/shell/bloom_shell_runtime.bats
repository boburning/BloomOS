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
    grep -F 'bloom_library_query_systems(database' "$source"
    grep -F 'bloom_library_query_recents(database, NULL' "$source"
    grep -F 'bloom_library_query_favorites(database, NULL' "$source"
    grep -F 'stage_game_with_core(&games[system->game_offset + system->focus.selected]' "$source"
    grep -F 'stage_game(&favorites[favorites_focus.selected]' "$source"
    grep -F 'stage_game(&recents[recent_focus.selected])' "$source"
    grep -F 'bloom_shell_stage_executable(GAME_SWITCHER_BINARY' "$source"
    grep -F 'destination == BLOOM_UI_DESTINATION_RECENT && recent_focus.item_count > 0' "$source"
    grep -F 'game_actions_open = 1;' "$source"
    grep -F 'bloom_shell_search_rebuild(&search, source, source_count' "$source"
    grep -F 'favorite_toggle(&selected_copy, favorites' "$source"
    grep -F 'bloom_ui_dialog_init(&recent_remove_dialog, 2, 0, 1)' "$source"
    grep -F 'gameswitcher_library_remove_recent(' "$source"
    ! grep -F 'navigation_open' "$source"
    ! grep -F 'MENU Navigate' "$source"
    ! grep -F 'draw_recent_actions' "$source"
}
