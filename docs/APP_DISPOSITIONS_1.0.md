# BloomOS 1.0 application dispositions

Bloom Shell is not a package-manager or settings overflow screen. Its Apps destination lists only
installed applications whose reviewed compatibility is `bloom-native` or `onion-compatible`.
`development-only` applications appear only while Developer Mode is active. Conservative
`mainui-dependent` imports remain installed for rollback compatibility but are not shown.

This table covers the application payloads carried by the 1.0 repository. “Stable Apps” describes
the 1.0 product surface, not whether an optional package remains available to developers or during
migration.

| Application payload | Disposition | Stable Apps | 1.0 ownership |
|---|---|---:|---|
| Activity Tracker | `KEEP_IN_APPS` | Yes | Reviewed Bloom-native play-history viewer; Recent remains the primary history surface. |
| Quick Guide | `KEEP_IN_APPS` | Yes | Reviewed Bloom-native offline help. |
| Ebook Reader (PixelReader) | `KEEP_IN_APPS` | After compatibility review | Optional content app; no settings responsibility. |
| Gallery (Screenshot viewer) | `KEEP_IN_APPS` | After compatibility review | Optional screenshot viewer. |
| PDF Reader (Green) | `KEEP_IN_APPS` | After compatibility review | Optional content app. |
| Random Game | `KEEP_IN_APPS` | After compatibility review | Optional game launcher action. |
| Video Player (FFplay) | `KEEP_IN_APPS` | After compatibility review | Optional media app. |
| Battery Monitor | `MIGRATE_TO_SETTINGS` | No | Battery is native, read-only Quick Settings/status data; battery policy belongs in Settings. |
| Clock (Set emulated time) | `MIGRATE_TO_SETTINGS` | No | Time synchronization and clock policy belong in Settings. |
| Onion OTA update | `MIGRATE_TO_SETTINGS` | No | Replaced by Bloom’s signed Update detail and rollback flow. |
| ThemeSwitcher | `MIGRATE_TO_SETTINGS` | No | Appearance belongs in flat Settings; Bloom keeps deterministic stable presentation. |
| AdvanceMENU (Alternative frontend) | `MIGRATE_TO_DEVELOPER` | No | Alternative frontend and maintenance surface. |
| Expert (Shortcut) | `MIGRATE_TO_DEVELOPER` | No | Package-management shortcut, never a normal destination. |
| Package Manager | `MIGRATE_TO_DEVELOPER` | No | Installation/maintenance authority, not a normal app. |
| RetroArch (Shortcut) | `MIGRATE_TO_DEVELOPER` | No | Advanced emulator configuration; ordinary launch stays supervised and game-first. |
| Terminal (Developer tool) | `MIGRATE_TO_DEVELOPER` | No | Explicitly `development-only`; visible only with Developer Mode. |
| GameSwitcher (Shortcut) | `REPLACE_WITH_NATIVE_BLOOM` | No | Global `MENU` owns GameSwitcher. |
| Tweaks | `REPLACE_WITH_NATIVE_BLOOM` | No | Flat Settings, Quick Settings, contextual actions, and bounded services replace normal use. Source remains migration/reference code only. |
| Guest Mode | `REMOVE_FROM_STABLE` | No | Legacy profile swap mutates duplicated state and is not a 1.0 product surface. |

## Stable staging policy

New stable images preinstall only Activity Tracker and Quick Guide. Upgrade cards may still contain
older or optional app directories; catalog compatibility filtering prevents Tweaks, Themes,
RetroArch shortcuts, Package Manager, and other unreviewed MainUI-dependent utilities from leaking
back into Bloom Shell. No package is deleted by this policy, preserving rollback and user data.

Tweaks parity is deliberately semantic rather than menu-for-menu. Display/audio/network controls,
RetroAchievements account handling, signed updates, health/support export, and developer gating are
owned by Bloom surfaces and bounded services. Legacy app sorting is removed because Bloom owns a
deterministic order. CUE/M3U, catalog-generation, reset, and diagnostic utilities remain migration
references until they receive a contextual or Developer-owned surface; their absence from Apps does
not weaken launch, save, update, recovery, or provenance guarantees.
