# MainUI Migration and Responsibility Map

Status: implementation-derived transition map for BloomOS 1.0. MainUI remains a
development recovery fallback until the corresponding Bloom owner is proven.

The machine-readable source of this inventory is
`build/mainui-responsibilities.json`. The shell gate validates that every entry
has a unique owner transition, a stable removal gate, and repository paths that
still exist. Update both representations whenever implementation moves an
ownership boundary.

## Current responsibility map

| Responsibility | Current evidence | Target owner | Removal gate |
|---|---|---|---|
| Main menu and tab state | `src/setState`, `write_mainui_state` callers | Bloom Shell navigation | Host navigation tests and Plus/V2/Flip UI evidence |
| Favorites | `/mnt/SDCARD/Roms/favourite.json`, keymon change detection | `bloom-library` | Idempotent import, canonical IDs, atomic writes |
| Recents and resume | MainUI recent list, GameSwitcher compatibility readers | Bloom activity/library | Deterministic import and direct resume |
| Game launch handoff | `/tmp` or `.tmp_update/cmd_to_run.sh`, MainUI termination | Bloom launch/session | Direct launch and shell return on hardware |
| MENU and context actions | `src/keymon/menuButtonAction.h` MainUI actions | Bloom input/shell | Stable control grammar and fallback |
| Brightness and volume | keymon plus shared settings helpers | `bloom-settings` and platform adapters | One writer and device tests |
| Network settings | Tweaks network menu and settings flags | Bloom network service/UI | Capability-gated Plus/Flip tests |
| Power, suspend, shutdown | keymon and Bloom shutdown scripts | Bloom power service | clean FAT, reboot, lid, recovery evidence |
| Theme installation | themeSwitcher plus MainUI-specific resources | Bloom theme adapter | malformed-theme fallback and UI parity |
| Health payload check | `bloom-health-system` currently requires MainUI | Bloom runtime health | green health with MainUI absent |
| Update activation | updater publishes a MainUI boot trigger/bind mount | Bloom boot/update trigger | signed activate/confirm/rollback without MainUI |
| App return | package launchers assume MainUI command handling | supervised app adapter | supported app matrix and direct shell return |

## Transition rules

1. Introduce a Bloom authority behind a versioned, testable interface.
2. Import legacy state read-only and preserve a rollback copy.
3. Switch one consumer at a time; do not allow indefinite dual writers.
4. Keep a bounded compatibility adapter only where firmware or supported apps
   still require the old boundary.
5. Prove host behavior, update/migration recovery, and physical device behavior.
6. Remove the stable dependency and classify remaining references as internal,
   migration, legal attribution, or development recovery.

`cmd_to_run.sh` may remain only as a documented firmware/application adapter.
Normal Bloom game launch must use the structured request and session services.
The stable return path is RetroArch or an app returning directly to Bloom Shell.

## Data safety

Imports are idempotent and transactional, reject traversal and symlinks, record
unmatched and duplicate entries, and never delete ROMs, BIOS, saves, states, or
media. MainUI state is not removed until the imported Bloom state and rollback
copy have been verified. Settings migrations preserve unknown legacy keys for
recovery but do not let them remain a second runtime authority.

## MainUI-free acceptance

With MainUI absent, boot, Home, library navigation, favorites, settings, launch,
game return, health, signed update activation/confirmation/rollback, shutdown,
and Safe Mode must pass. Development fallback behavior is reported separately
and cannot satisfy this gate.
