#!/usr/bin/env bats

@test "Tweaks exposes the canonical RetroAchievements settings surface" {
    grep -F '.label = "RetroAchievements..."' /workspace/src/tweaks/menus.h
    grep -F '.label = "Enable achievements"' /workspace/src/tweaks/retroachievements.h
    grep -F '.value_labels = {"Softcore", "Hardcore"}' /workspace/src/tweaks/retroachievements.h
    grep -F '.label = "Offline Casual"' /workspace/src/tweaks/retroachievements.h
    grep -F '.label = "Offline cache..."' /workspace/src/tweaks/retroachievements.h
    grep -F 'Offline awards pending:' /workspace/src/tweaks/retroachievements.h
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
    ! grep -E 'cheevos_token|credentials.append|username|cJSON.*token' "$file"
}
