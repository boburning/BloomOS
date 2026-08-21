#!/usr/bin/env bats

@test "Tweaks exposes the canonical RetroAchievements settings surface" {
    grep -F '.label = "RetroAchievements..."' /workspace/src/tweaks/menus.h
    grep -F '.label = "Enable achievements"' /workspace/src/tweaks/retroachievements.h
    grep -F '.value_labels = {"Softcore", "Hardcore"}' /workspace/src/tweaks/retroachievements.h
    grep -F '.label = "Offline Casual"' /workspace/src/tweaks/retroachievements.h
    grep -F '.label = "Offline cache..."' /workspace/src/tweaks/retroachievements.h
    grep -F 'Offline awards pending:' /workspace/src/tweaks/retroachievements.h
    grep -F '"Sign in"' /workspace/src/tweaks/retroachievements.h
}

@test "Tweaks delegates mutations to bounded bloomctl commands" {
    file=/workspace/src/tweaks/retroachievements.h
    grep -F 'account set enabled true' "$file"
    grep -F 'account set enabled false' "$file"
    grep -F 'account set mode softcore' "$file"
    grep -F 'account set mode hardcore' "$file"
    grep -F 'account set offline-casual automatic' "$file"
    grep -F 'account set offline-casual disabled' "$file"
    grep -F 'account sign-out' "$file"
    grep -F 'scan --changed' "$file"
    grep -F 'proxy cache-favorites' "$file"
    grep -F 'proxy cache-recent' "$file"
    grep -F 'proxy cache-all' "$file"
}

@test "Tweaks consumes only redacted account and aggregate proxy fields" {
    file=/workspace/src/tweaks/retroachievements.h
    grep -F 'account status' "$file"
    grep -F 'proxy pending' "$file"
    grep -F '"configured"' "$file"
    grep -F '"authenticated"' "$file"
    grep -F '"pending_awards"' "$file"
    ! grep -E 'cheevos_token|credentials.append|cJSON.*(token|username)' "$file"
}

@test "Tweaks masks passwords and submits login only through fixed stdin" {
    file=/workspace/src/tweaks/retroachievements.h
    keyboard=/workspace/src/tweaks/text_entry.h
    grep -F "shown[i] = masked ? '*'" "$keyboard"
    grep -F 'execl("/mnt/SDCARD/.tmp_update/bin/bloomctl", "bloomctl", "achievements", "account", "login"' "$file"
    grep -F 'memset(password, 0, sizeof(password));' "$file"
    ! grep -E 'system\(.*(username|password)|popen\(.*(username|password)' "$file"
}

@test "Tweaks text entry provides a visible QWERTY ASCII keyboard" {
    keyboard=/workspace/src/tweaks/text_entry.h
    grep -F 'CHAR_KEY(q), CHAR_KEY(w), CHAR_KEY(e), CHAR_KEY(r), CHAR_KEY(t), CHAR_KEY(y)' "$keyboard"
    grep -F 'NAMED_KEY("Shift", TEXT_KEY_SHIFT)' "$keyboard"
    grep -F 'NAMED_KEY("123!?", TEXT_KEY_SYMBOLS)' "$keyboard"
    grep -F 'NAMED_KEY("Bksp", TEXT_KEY_BACKSPACE)' "$keyboard"
    grep -F 'NAMED_KEY("Space", TEXT_KEY_SPACE)' "$keyboard"
    grep -F 'NAMED_KEY("Done", TEXT_KEY_DONE)' "$keyboard"
    grep -F 'SDL_FillRect(screen, &key_rect, selected ? selected_color : key_color);' "$keyboard"
}

@test "Tweaks resets the framebuffer page before SDL rendering" {
    run python3 - <<'PY'
from pathlib import Path

source = Path('/workspace/src/tweaks/tweaks.c').read_text(encoding='utf-8')
reset = source.index('display_init(false);')
sdl = source.index('SDL_InitDefault();', reset)
assert reset < sdl
PY
    [ "$status" -eq 0 ]
}
