# Compatibility Policy

BloomOS 1.x aims to preserve existing Onion ROM and BIOS layouts, saves, compatible save states, favorites, recents, themes, icon packs, RetroArch configuration, and applications that do not rely on undocumented device assumptions.

Required migrations must be versioned, idempotent, backed up, logged, validated against fixtures, and recoverable. BloomOS will not promise arbitrary downgrade compatibility, but recovery must preserve user-owned ROMs, BIOS files, saves, and media.

Compatibility breaks require explicit rationale, migration notes, tests, and a rollback or recovery path.
