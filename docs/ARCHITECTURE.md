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
The developer game probe exercises and reports the method that completed. This is not yet the
complete lifecycle manager: standalone-emulator adapters, save-flush
confirmation, crash recovery, and selective service suspension remain separate
milestones.

`bloom-save-flush` provides the native durability boundary for RetroArch saves.
It accepts a validated core basename, resolves that core's canonical `corename`
from RetroArch metadata, and flushes only the matching save and state trees plus
their immediate parent directory entries. It never invokes a global `sync` or
walks another core's data. Missing core metadata, unsafe names, symlinks, and
non-regular entries fail closed. Directory `fsync` returning `EINVAL` is allowed
for FAT compatibility; regular save and state files must still flush
successfully. Session integration remains responsible for preventing `STOPPED`
until this scoped operation succeeds.

Architectural decisions will be recorded as the corresponding subsystem work begins.
