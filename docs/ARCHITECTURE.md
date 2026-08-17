# Architecture

## Direction

BloomOS retains Onion's existing layout initially and replaces fragile boundaries incrementally. Large repository moves are not a bootstrap goal.

The target architecture separates shared services from original Mini, Plus, and Flip platform backends. The original Mini backend must represent V1–V4 hardware and firmware differences explicitly; it must not assume that the maintainer-owned V2 is representative of every revision. Consumers should query explicit capabilities—such as display dimensions, Wi-Fi, RTC, lid, and rumble—instead of scattering model checks.

Planned stable service boundaries include platform discovery, structured game launch, session lifecycle, canonical game identity, activity storage, updates, sync, and health diagnostics. Miyoo's closed MainUI remains a compatibility boundary for BloomOS 1.x.

## BloomPlatform foundation

`bloom-platform` is the first device-side capability boundary. Its schema-1,
read-only inspection interface centralizes strong-signal family detection,
physical display normalization, Wi-Fi, developer SSH, lid, RTC, input presence,
input-device count, and the available battery-reading backend. Consumers must
query those capabilities instead of inferring them from scattered numeric model
checks. Unknown hardware remains `unknown`; the maintainer-owned Mini V2 does
not authorize guessing an original Mini V1–V4 revision.

The interface intentionally reports facts without changing hardware state.
Brightness, clocks, key maps, radios, lid behavior, and power controls remain
outside this initial boundary until each operation has a capability guard,
restore path, and device-matrix evidence. `bloomctl` and the guarded original
Mini test runner are the first migrated consumers. The vendor runtime remains a
compatibility consumer and will be migrated incrementally after live parity is
established.

## Structured launch boundary

`bloom-launch` owns schema-1 `BloomLaunchRequest` validation and atomic request
writes. ROM and launcher paths, emulator type, core, automatic-state intent,
temporary configs, requested resolution, and environment are data fields; new
Bloom components must not construct executable shell text from them.

Miyoo's closed MainUI still requires `cmd_to_run.sh`. The `write-legacy`
operation is the only supported compatibility adapter: it validates the full
request, confines ROMs and launchers to their expected SD-card roots, rejects
duplicate/unknown fields and traversal, then atomically writes the legacy
command in the exact double-quoted shape expected by the vendor runtime. The
adapter currently refuses double quote, backtick, and backslash in a path
because the legacy runtime cannot round-trip them safely. Structured requests
retain the broader data model so that limitation disappears with the legacy
parser rather than leaking into Bloom's API.

`bloom-session` owns the initial explicit lifecycle state machine. It validates
and snapshots the immutable launch request, hashes that snapshot, serializes
writers with an atomic lock, rejects stale or skipped transitions, attaches a
verified emulator PID, and exposes machine-readable terminal state. The only
successful path is `PREPARING → STARTING → RUNNING → STOP_REQUESTED → FLUSHING
→ STOPPED`; an active state may instead terminate as `FAILED` with a bounded
data-safe reason.

The first session-lifecycle action invariant is owned by `bloom-session
stop-retroarch`: request `QUIT` through the bounded localhost control client,
wait for exit, then use SIGTERM and finally SIGKILL only as bounded fallbacks.
A forced kill is a `FAILED` terminal session rather than successful flushing.
The developer game probe exercises and reports the method that completed. This
is not yet the complete lifecycle manager: standalone-emulator adapters, crash
recovery, and selective service suspension remain separate milestones.

Session elapsed time is measured from the emulator's successful `attach` to
its confirmed exit using the monotonic kernel uptime clock. The start and final
whole-second duration are atomically persisted with the session. A missing,
invalid, or regressed monotonic clock makes the session `FAILED`; wall-clock
RTC or NTP changes are never consulted and therefore cannot create fake play
time. Writing this owned duration into Play Activity is a subsequent adapter
step while legacy MainUI callers still own their existing activity rows.

The native `playActivity stop-duration` adapter accepts an already measured
elapsed duration, closes only the newest matching open row, and uses wall time
only for the row's `updated_at` metadata. It is intentionally not called by the
session manager until activity-start ownership moves away from the legacy
MainUI/keymon paths; this prevents duplicate or competing activity rows during
the compatibility transition.

`bloom-save-flush` provides the native durability boundary for RetroArch saves.
It accepts a validated core basename, resolves that core's canonical `corename`
from RetroArch metadata, and flushes only the matching save and state trees plus
their immediate parent directory entries. It never invokes a global `sync` or
walks another core's data. Missing core metadata, unsafe names, symlinks, and
non-regular entries fail closed. Directory `fsync` returning `EINVAL` is allowed
for FAT compatibility; regular save and state files must still flush
successfully. `bloom-session` snapshots the validated core with the immutable
request and prevents `STOPPED` until this scoped operation succeeds and its
result is atomically recorded. A flush failure is terminal `FAILED`, so a clean
exit can never be reported without save durability confirmation.

## Canonical game identity

`bloom-game-id` defines the schema-1 identity boundary. It normalizes the ROM
path relative to `/mnt/SDCARD/Roms`, rejects traversal and unsafe paths, and
hashes `bloom-game-v1`, the stable system identifier, and that relative path as
three NUL-separated UTF-8 fields. The public identifier is
`bloom-game-v1:<sha256>`; JSON inspection also exposes the source system and
normalized path for debugging and migration. Identity never depends on display
title, basename, or extension-stripped filename. A playlist or container path
therefore remains the canonical identity for a multi-disc title when it is the
launched ROM path.

Structured launch validation recomputes the identifier and rejects a request
whose GameID does not match its system and ROM path. New launch consumers must
obtain the identifier from `bloom-game-id`; the legacy MainUI recent list
remains a compatibility data source pending GameSwitcher and activity migration.

## Play Activity data boundary

The first Play Activity cleanup isolates SQLite row ownership and image-path
derivation in `playActivityModel`. Result columns are mapped explicitly to the
nine-column aggregate query, nullable values are initialized, and all nested
ROM strings have one matching cleanup path. Image lookup is non-mutating:
ordinary ROMs map to the system `Imgs` directory while PICO-8 `.p8` and `.png`
cartridges remain their own images. Canonical GameID columns, schema migrations,
and monotonic duration tracking are subsequent additive database changes; this
slice intentionally preserves the existing database and aggregate totals.

Schema version 1 adds singleton `schema_version` state, append-only
`migration_history`, and a nullable unique canonical `rom.game_id` column. The
migration uses `BEGIN IMMEDIATE`, preserves a SQLite-native pre-v1 backup before
touching an existing database, rolls back on failure, is idempotent, and refuses
newer or internally incomplete schemas. Identity backfill is intentionally not
part of this migration: ambiguous legacy rows remain `NULL` until their source
system and normalized ROM path can be established deterministically.

The explicit `backfill-game-ids` command currently recognizes only the six
device-proven mappings (`GB`, `GBC`, `GBA`, `FC`, `SFC`, and `PS`). It supports a
non-mutating dry run, precomputes duplicate identities, and defers unknown,
duplicate, conflicting, or unsafe legacy rows while updating unrelated rows in
one transaction. Repeated application is idempotent. Expanding the mapping set
requires system-specific launch evidence; folder names are never blindly
lowercased into identity.

`playActivity health` is the read-only database health boundary. It validates
the complete schema-1 invariants, runs SQLite `PRAGMA quick_check`, and reports
orphan activity rows, negative durations, currently open sessions, and
unidentified ROM rows as structured JSON. Integrity failures, orphans, and
negative durations make the command fail; open sessions and intentionally
deferred identities remain operational counts rather than corruption.

`bloomctl health` is the stable system-facing diagnostics entry point. Its
schema-1 report embeds the Play Activity report without rewriting it and
propagates an unhealthy or unavailable database check through its exit status.
Additional independently testable subsystem checks can be added under the
top-level `checks` object without changing the native database boundary.

Architectural decisions will be recorded as the corresponding subsystem work begins.
