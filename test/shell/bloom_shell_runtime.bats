#!/usr/bin/env bats

@test "Bloom Shell is default and MainUI requires an explicit developer recovery gate" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    grep -F 'elif mainui_development_fallback_enabled; then' "$runtime"
    grep -F 'if mainui_development_fallback_enabled && [ -f /tmp/run_advmenu ]; then' "$runtime"
    grep -F '[ -f "$bloom_developer_marker" ] && [ ! -L "$bloom_developer_marker" ] &&' "$runtime"
    grep -F '[ -f "$sysdir/config/.mainuiFallback" ] && [ ! -L "$sysdir/config/.mainuiFallback" ]' "$runtime"
    grep -F 'if mainui_development_fallback_enabled && ! bloom_shell_safe_mode_pending &&' "$runtime"
    grep -F 'Retrying Bloom Shell through the guarded Safe Mode threshold' "$runtime"
    grep -F '"$sysdir/bin/bloom-shell" >/dev/null 2>&1' "$runtime"
    grep -F '[ "$shell_status" -eq 20 ] && [ -f "$sysdir/cmd_to_run.sh" ]' "$runtime"
    ! grep -F '[ -f "$sysdir/config/.bloomShell" ]' "$runtime"
}

@test "runtime dispatch cannot enter MainUI with only one recovery gate" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    sysdir="$BATS_TEST_TMPDIR/system"
    bloom_developer_marker="$BATS_TEST_TMPDIR/.bloom-dev"
    mkdir -p "$sysdir/config"
    calls="$BATS_TEST_TMPDIR/calls"

    launch_bloom_shell() { printf 'bloom\n' >> "$calls"; }
    launch_main_ui() { printf 'mainui\n' >> "$calls"; }
    check_off_order() { :; }
    eval "$(sed -n '/^check_main_ui() {/,/^bloom_shell_safe_mode_pending() {/p' "$runtime" | sed '$d')"

    check_main_ui
    [ "$(tail -n 1 "$calls")" = bloom ]
    touch "$sysdir/config/.mainuiFallback"
    check_main_ui
    [ "$(tail -n 1 "$calls")" = bloom ]
    rm "$sysdir/config/.mainuiFallback"
    touch "$bloom_developer_marker"
    check_main_ui
    [ "$(tail -n 1 "$calls")" = bloom ]
    touch "$BATS_TEST_TMPDIR/fallback-target"
    ln -s "$BATS_TEST_TMPDIR/fallback-target" "$sysdir/config/.mainuiFallback"
    check_main_ui
    [ "$(tail -n 1 "$calls")" = bloom ]
    rm "$sysdir/config/.mainuiFallback"
    touch "$sysdir/config/.mainuiFallback"
    check_main_ui
    [ "$(tail -n 1 "$calls")" = mainui ]
}

@test "boot reconciles signed application declarations before Bloom Shell reads the catalog" {
    runtime=/workspace/static/build/.tmp_update/runtime.sh
    grep -F 'load_settings' "$runtime"
    grep -F 'reconcile_bloom_library' "$runtime"
    grep -F '"$sysdir/bin/bloomctl" library import-onion > /dev/null 2>&1' "$runtime"

    load_line="$(grep -n '^[[:space:]]*load_settings$' "$runtime" | cut -d: -f1)"
    library_line="$(grep -n '^[[:space:]]*reconcile_bloom_library$' "$runtime" | cut -d: -f1)"
    [ "$load_line" -lt "$library_line" ]
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

@test "Bloom Shell visual system uses native Mini and Flip geometry without fake text" {
    shell=/workspace/src/bloomShell/main.c
    renderer=/workspace/src/bloomUi/bloom_ui_renderer.c
    grep -F 'draw_root_icon(screen' "$shell"
    grep -F 'draw_game_preview(screen' "$shell"
    grep -F '.row_width_percent = game_destination ? 58 : 100' "$shell"
    grep -F 'battery_capacity_available' "$shell"
    grep -F 'compact_font' "$shell"
    grep -F '"A Open   B Home   START Quick"' "$shell"
    ! grep -F 'Deterministic text placeholders' "$renderer"
    ! grep -E 'L1|L2|R1|R2' "$shell"
}

@test "reviewed PICO-8 and ScummVM systems use shipped structured RetroArch cores" {
    games=/workspace/src/bloomShell/bloom_shell_games.c
    cores=/workspace/static/build/RetroArch/.retroarch/cores

    grep -F '{"pico8", "fake08_libretro.so"}' "$games"
    grep -F '{"scummvm", "scummvm_libretro.so"}' "$games"
    [ -f "$cores/fake08_libretro.so" ]
    [ -f "$cores/scummvm_libretro.so" ]
    grep -F 'EXPECT_EQ(nullptr, bloom_shell_games_default_core("ports"));' \
        /workspace/test/test_bloom_shell_games.cpp
}
