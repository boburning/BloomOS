# Architecture

## Direction

BloomOS retains Onion's existing layout initially and replaces fragile boundaries incrementally. Large repository moves are not a bootstrap goal.

The target architecture separates shared services from original Mini, Plus, and Flip platform backends. The original Mini backend must represent V1–V4 hardware and firmware differences explicitly; it must not assume that the maintainer-owned V2 is representative of every revision. Consumers should query explicit capabilities—such as display dimensions, Wi-Fi, RTC, lid, and rumble—instead of scattering model checks.

Planned stable service boundaries include platform discovery, structured game launch, session lifecycle, canonical game identity, activity storage, updates, sync, and health diagnostics. Miyoo's closed MainUI remains a compatibility boundary for BloomOS 1.x.

## RetroAchievements boundary

`bloom-ra` is the planned schema-versioned RetroAchievements service boundary.
It attaches authoritative rcheevos content identity and minimal RA metadata to
canonical Bloom GameID without replacing that identity. Game support, exact
core-binary certification, and direct/proxy transport are independent policy
dimensions. UI consumers read persistent local metadata and never hash or use
the network during badge rendering.

RA launch policy extends `BloomLaunchRequest` and is owned for the immutable
session lifetime. Bloom writes a temporary RetroArch append config; it never
uses permanent config mutation as normal control flow. Hardcore is direct-only
and fails explicitly when its prohibited-feature invariants cannot be
guaranteed. Optional RAOfflineProxy is a replaceable softcore transport behind
`bloom-ra-proxy`, not a dependency of `bloom-ra` or ordinary game launch. The
complete contract and ordered delivery plan are in
[`RETROACHIEVEMENTS.md`](RETROACHIEVEMENTS.md).

## Bloom settings boundary

`bloom-settings` is the versioned durable owner being introduced for global
and device settings. Its schema-1 state lives outside `.tmp_update`, publishes
with a restrictive atomic replace, and refuses corrupt or newer schemas rather
than guessing. The initial Onion importer reads only fixed compatibility paths,
retains unknown `system.json` and keymap data inside the compatibility record,
and stores an exact first-import `system.json` snapshot for rollback evidence.
It is idempotent: a valid Bloom settings file is never replaced by a later
legacy import. The file explicitly records whether `legacy` compatibility or
`bloom` is the active writer, preventing a compatibility sync from overwriting
Bloom-owned state after cutover.

During migration, Onion files remain untouched and existing consumers continue
to use them. A serialized direct sync may refresh known canonical values only
while authority remains `legacy`; it preserves unknown canonical fields,
increments generation only for a real change, and is rejected after Bloom
cutover. Existing Onion save paths invoke the service-owned compatibility
reconciler through a fixed `fork`/`execl` boundary; they never construct a shell
command, and a missing or inactive Bloom service cannot prevent the legacy save
from completing. That reconciler remains available after cutover so unavoidable
closed-MainUI writes can be committed back into canonical state during the
transition. The presence of an imported schema alone is never treated as
authority cutover.

Schema 1 separates durable settings into `device`, `interface`, `behavior`, and
`controls` objects. The imported model covers every durable value currently
consumed by keymon and Tweaks, while the `compatibility` object retains unknown
Onion JSON for lossless transition. Runtime observations such as RTC presence,
network association, and battery state are deliberately excluded; consumers
obtain those capabilities from `bloom-platform` instead of persisting them as
user settings.

While authority remains `legacy`, normal boot performs an idempotent import and
sync after Onion has loaded its per-device `system.json`. Keymon reconciles
shared-memory settings changes after persisting them, and the WPS path reconciles
its explicit Wi-Fi preference update. Each bridge is optional and fail-open for
legacy behavior; a missing or unhealthy canonical service cannot block boot,
input handling, networking, or game launch.

After authority cutover, `bloom-settings materialize-onion` derives the closed
MainUI compatibility files from the typed canonical model. It validates every
canonical field and preflights every output target before publication, refuses
to run while Onion remains authoritative, and preserves unknown imported JSON.
Canonical state commits independently; the legacy `system.json`, keymap, flags,
and scalar files are recoverable projections that repeated materialization can
converge after an interrupted fan-out.

Cutover uses `activate-bloom`, which first reconciles the latest legacy values,
atomically increments the canonical generation while changing authority, and
then materializes the compatibility projection. A failed first materialization
performs a generation-guarded rollback to `legacy`; it cannot overwrite an
intervening canonical commit. `rollback-authority` provides the explicit reverse
transition. Repeating either operation is idempotent.

Bloom-owned consumers mutate settings only through `set FIELD VALUE`. Its
checked-in allowlist assigns each supported field an exact type and range,
requires active Bloom authority, publishes the canonical mutation atomically,
and then materializes Onion compatibility state. If projection fails, the
canonical commit remains authoritative and diagnostics report that it still
needs materialization. Arbitrary JSON paths and unbounded values are never
accepted through the public CLI.

## Bloom library boundary

`bloom-library` is the durable indexed owner for systems, games, applications,
favorites, and legacy migration evidence. Its SQLite catalog lives at
`/mnt/SDCARD/.bloom/library/catalog.sqlite3`, outside the replaceable runtime
tree. Schema migrations are transactional, reject a newer schema, refuse a
symlinked database path, and retain unmatched or duplicate Onion items as
explicit migration results rather than guessing or discarding them.

The schema keeps canonical Bloom GameID separate from display metadata and from
the independent RetroAchievements index. Systems and applications retain the
fixed compatibility paths needed to launch current packages; games retain file
size and mtime for incremental invalidation. Rows use a `present` marker so a
transactional scan can publish one generation without deleting useful prior
metadata while enumeration is incomplete. UI consumers page and sort through
local queries only; drawing a library row must never scan the ROM tree or use
network I/O.

## Bloom Shell vertical slice

`bloom-shell` is a native SDL consumer of the Bloom UI, library, launch, and
session boundaries. Its first development slice loads the Game Boy catalog
through bounded SQLite cursor pages before entering the render loop. Drawing
and navigation use only in-memory rows; no subprocess, ROM scan, or network
request occurs on the render path.

Confirming a game creates a schema-1 structured request, asks `bloom-session`
to own the immutable session, and publishes the existing quoted command adapter
only after validation. Runtime opt-in is controlled by the development-only
`.tmp_update/config/.bloomShell` flag. A successful staged launch exits with the
fixed status 20 so the existing runtime launches the game and returns to Bloom
Shell on completion. The fixed session runner attaches the observed RetroArch
PID, records a monotonic natural exit, flushes scoped saves, and completes the
session before control returns to the runtime. Any other exit or crash removes incomplete shell-owned
handoff files and immediately starts MainUI as the recovery fallback. This is
not yet the stable default path.

The public boundary exposes `bloomctl library status` and the explicit
`bloomctl library import-onion` mutation. The importer reads a signed package
catalog that maps Onion emulator folders to stable Bloom system IDs, validates
each installed system/application config without following symlinks, normalizes
only fixed SD-card path prefixes, and atomically publishes a new generation.
Repeated imports are no-ops; malformed or empty discovery cannot replace the
prior known-good catalog. The signed mapping distinguishes compatibility quirks
such as the `PSX` emulator folder using `Roms/PS` without relaxing the path
boundary.

Application rows carry one explicit compatibility class: `bloom-native`,
`onion-compatible`, `mainui-dependent`, or `development-only`. Schema 4
conservatively migrates every existing row to `mainui-dependent`. Onion app
imports keep that default unless the reviewed config declares a valid
`bloom_compatibility` value; unknown values abort the transaction. Queries
return the class with the launch path so consumers can enforce policy without
guessing from an app name or script contents.

Bloom Shell owns a capability-filtered settings presentation rather than
reusing the legacy Tweaks menu. The top-level model exposes Display, Audio,
Controls, Gameplay, RetroAchievements, Appearance, and System on every
supported device; Network appears only on Plus and Flip, and Advanced appears
only while Developer Mode is active. Every category opens a bounded detail
page and Back restores the prior top-level selection. Display, Audio, and
Network reuse the same event-driven fixed adapters as Quick Settings; Controls,
Gameplay, Appearance, and Advanced expose stable plain-language policy values.
System and RetroAchievements retain bounded status summaries loaded before the
render loop, while System also identifies the installed BloomOS version. START
opens a compact Quick Settings model from any normal shell destination;
Original Mini omits Wi-Fi while Plus and Flip include it. The shell loads one
bounded canonical value snapshot before entering the render loop and displays
explicit unavailable state if that read fails. Left and right requests are
event-driven fixed-argument executions of the Bloom brightness, volume, and
network adapters; the in-memory value changes only after the adapter succeeds.
No settings subprocess runs while rendering, and arbitrary fields or commands
cannot cross this boundary.

Battery state remains outside canonical settings. `bloom-platform battery
--json` normalizes sysfs, batmon, and AXP cache/live readings into a bounded
capacity/charging response and rejects malformed or out-of-range values. Bloom
Shell loads that snapshot before rendering alongside the canonical settings
snapshot, so the Quick Settings battery row does not duplicate hardware model
heuristics.

The canonical service exposes `bloomctl settings values` as the bounded,
read-only bridge for future UI consumers. Its schema-1 response includes only
generation, authority, brightness, volume, mute, and Wi-Fi state; it does not
serialize themes, control mappings, account data, credentials, or preserved
legacy fields. Mutations continue through the existing allowlisted control and
network adapters, which persist through the canonical `set` boundary, rather
than accepting arbitrary JSON.

Bloom Shell applies that policy at a single supervised launch boundary. Only
`bloom-native` and reviewed `onion-compatible` rows may stage a command;
`mainui-dependent` and `development-only` rows remain visible but cannot launch
from the ordinary shell. The launcher must be a non-symlink executable regular
file below `App/`. Its absolute path is shell-quoted as data in a mode-0700
temporary file, flushed, atomically renamed to the runtime command path, and
followed by a parent-directory flush. No application arguments are accepted.
After staging, exit status 20 transfers control to the existing runtime loop,
which executes the application and returns to Bloom Shell.

Game enumeration and consumer queries live behind the same service.
`bloomctl library scan --changed|--all|--system SYSTEM`
enumerates only extensions declared by the imported system configuration,
rejects symlink traversal, derives canonical Bloom GameID without reading ROM
contents, and atomically invalidates only the systems included in that scan.
File size, mtime, display metadata, and presence changes advance one catalog
generation; an unchanged repeat is a no-op.

`bloomctl library games` provides a bounded local page of at most 100 present
games, optionally filtered by system. Its GameID cursor resumes the stable
`sort_title, bloom_game_id` ordering without exposing SQL or allowing a UI
consumer to initiate filesystem or network work. Schema 2 adds and validates a
global paging index through an additive transaction while retaining the
system-specific index.

`bloomctl library import-legacy` reads the newline-delimited Onion favorites and
active recent list without modifying either source. Known historical game types
and colon-packed launcher/ROM entries are normalized to the already indexed ROM
path, then matched by canonical Bloom GameID. The transaction publishes compact
canonical `favorites` and `recents` order while retaining every source position
as `matched`, `unmatched`, `duplicate`, or `invalid` migration evidence. Missing
lists mean empty state; an unsafe list or database failure preserves the prior
known-good state. Schema 3 adds canonical recents through an additive migration.

The Bloom Shell GB vertical slice is the first canonical recent and favorite-state
consumer. Its Home screen queries the ordered `recents` relation directly and
offers one-confirmation resume for the latest present GB game. Collections reads
the ordered `favorites` relation and launches a selected present GB game through
the same structured boundary. L1/R1 cycle only through the implemented Home,
Library, and Collections destinations; Apps and Settings remain unreachable until
they have real backing models. Both bounded queries omit missing games and systems
and do not parse or mutate the MainUI lists. MainUI remains the compatibility writer
until Bloom-owned mutation, signed-update/reboot, and physical launch/return paths
are proven; these read-only consumers do not create a second writer.

## BloomPlatform foundation

`bloom-platform` is the first device-side capability boundary. Its schema-1,
read-only inspection interface centralizes strong-signal family detection,
physical display normalization, Wi-Fi, developer SSH, lid, RTC, input presence,
input-device count, and the available battery-reading backend. Consumers must
query those capabilities instead of inferring them from scattered numeric model
checks. Unknown hardware remains `unknown`; the maintainer-owned Mini V2 does
not authorize guessing an original Mini V1–V4 revision.

The interface intentionally reports facts without changing hardware state.
Brightness, clocks, key maps, radios, and lid behavior remain outside this
initial boundary until each operation has a capability guard, restore path, and
device-matrix evidence. `bloomctl` and the guarded original Mini test runner are
the first migrated consumers. The vendor runtime remains a compatibility
consumer and will be migrated incrementally after live parity is established.

## Bloom power boundary

`bloom-power` is the versioned policy adapter above the existing low-level clean
shutdown implementation. Its read-only status reports the platform-derived
poweroff strategy: original Mini firmware uses reboot for its poweroff handoff,
while Plus and Flip use poweroff. Unknown hardware and missing dependencies fail
closed. The only mutations are fixed `request reboot` and `request poweroff`
operations; arbitrary commands or arguments cannot cross the boundary.

`bloomctl power` exposes that contract to Bloom-owned consumers. Automatic
update rollback, runtime shutdown, keymon forced shutdown, and the charging UI
call the adapter rather than the low-level helper directly. The low-level helper
continues to own SD-backed swap shutdown, settings flush, FAT read-only remount,
recursive unmount, charging-mode
bypass, and firmware syscall fallbacks that already have physical Plus evidence.
Suspend/lid behavior, brightness, volume, clock, and network consumers remain
explicit follow-up migrations; this first slice does not broaden their claims.

## Bloom network boundary

`bloom-network` owns the schema-1, privacy-bounded local network status used by
Bloom consumers. It combines `bloom-platform` Wi-Fi capability, the firmware's
current Wi-Fi preference, and read-only `wlan0` association signals without
exposing an SSID, address, credential, or network scan. Original Mini hardware
reports `no_network_hardware` immediately instead of waiting on an interface
that cannot exist; enabled but disconnected radios report `not_associated`.

The service intentionally does not claim internet reachability and performs no
network I/O. `bloom-ra-network` consumes its local state and then adds the
separate bounded clock, DNS, TLS, and RetroAchievements service probe. This
keeps generic hardware/association facts independent from RA transport policy.

Normal runtime reconciliation also enters through the fixed
`bloom-network request reconcile` operation. Network-capable hardware delegates
Plus radio, association, and DHCP lifecycle to the fixed `bloom-wifi` backend.
An already-associated connection is preserved without restarting either
network client. The inherited script is restricted to auxiliary compatibility
services; unsupported Wi-Fi hardware can still fall back to its complete
`check` action until a Bloom backend is physically proven for that device.
Hardware without networking is a successful no-op.

Bounded `request enable` and `request disable` operations persist only
`device.wifi_enabled` through the active `bloom-settings` authority before
applying the same fixed Bloom backend action. Settings rejection is distinct from an
apply failure, and a successfully saved preference remains durable for later
reconciliation if immediate application fails. Hardware without Wi-Fi rejects
the mutation before touching settings. Flip lifecycle ownership and replacement
of the remaining inherited auxiliary services remain follow-up work.

## Bloom controls boundary

`bloom-controls` is the model-aware brightness and volume runtime-control
boundary. `status` reports only the known model, bounded raw PWM state, and
whether the firmware audio-server FIFO is available.
The public brightness request accepts exactly 0 through 10, persists only
`device.brightness` through `bloom-settings`, and applies Bloom's single
canonical logical-to-PWM curve. Settings rejection remains distinct from a
later hardware-apply failure.

The separate internal `apply brightness` operation never persists state. Boot
uses it after preparing the PWM period, removing the duplicated curve and raw
duty-cycle write from `runtime.sh`. Public volume requests accept exactly 0
through 20, persist the canonical level and cleared mute intent, and map it to
the inherited logarithmic -60 through +3 dB curve. A small source-built
`bloom-volume` helper writes the Plus firmware audio server's fixed 24-byte
atomic FIFO request; it rejects linked/non-FIFO endpoints and never waits
indefinitely for a missing reader. Keymon retains the direct device path where
firmware exposes it and uses this absolute-volume fallback on Plus audio-server
firmware. Public volume requests preflight the backend before changing
settings; non-Plus public backends remain follow-up work. Relative audio boost
remains unavailable on that fallback because the server does not expose a safe
current-level query. Unknown hardware and unavailable controls fail closed.

## Bloom time boundary

`bloom-time` owns capability-aware wall-clock status and boot reconciliation.
It distinguishes RTC presence from whether the clock is usable. A valid RTC or
already-usable system clock is preserved without setting time. Hardware without
RTC may restore only a regular, bounded saved epoch; the compatibility offset
is range-checked and suppressed when network time is enabled. Missing, linked,
or malformed fallback data never reaches `date`.

Before the no-RTC path changes the final wall clock, it requires and closes the
legacy Play Activity boundary. Boot invokes only `bloom-time reconcile` and
continues with a bounded diagnostic if time cannot be restored. The public
`bloomctl time status` surface is read-only; normal UI rendering never performs
time or network mutation.

## Bloom lid boundary

`bloom-lid status` is the read-only lid-state boundary. It normalizes both the
current Flip `/sys/devices/platform` hall-sensor layout and the earlier
`/sys/devices/soc0` layout, reports only open/closed state, and rejects linked
or malformed sensor data. A Flip identity with no readable sensor is an error;
hardware without lid capability reports `unsupported`. `bloom-platform` and
early model detection consume the same two observed layouts. Lid-triggered
suspend remains separate and must not land until the state boundary is
physically validated on Flip hardware.

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

Until every emulator launcher consumes structured requests directly,
reset/forced-auto-load options cross the compatibility boundary through
`bloom-launch-override`. It validates the permanent launcher and the two
supported temporary configs, writes a mode-0700 copy beside the launcher so
`$0`-relative paths remain valid, and injects the option only into that copy.
Runtime cleanup removes the Bloom-owned copy and never edits the permanent
`launch.sh`; interruption can therefore leave at most an inert hidden file,
not a corrupted emulator definition.

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

`bloom-save-snapshot` protects the complete current-profile `saves` and
`states` trees. Creation rejects links and special files, archives both trees
into a staging directory, records a SHA-256 checksum, and publishes only after
a completion marker verifies. Restore is explicit, refuses active sessions,
verifies the archive before touching live data, and moves both current trees
to an on-card rollback directory until both replacements succeed. An
interrupted restore therefore leaves the prior trees recoverable rather than
silently reporting success.

Snapshot creation applies a default retention limit of five verified snapshots
under the same operation lock. Retention orders Bloom-owned safe identifiers,
keeps the newest verified snapshots, and additionally preserves any snapshot
referenced by active update or rollback state. Corrupt, incomplete, linked, or
otherwise unverifiable entries are never silently deleted as retention. The
explicit `prune KEEP` operation uses the same rules and rejects zero or
non-numeric limits before removal.

The read-only `list` operation returns schema-1 structured inventory with each
safe snapshot's identifier, trigger, archive bytes, verification status, and
active-update reference status. `bloomctl saves snapshots` exposes only that
inventory for diagnostics and future UI consumers; restore and prune remain
explicit lower-level operations and are not reachable through this read-only
CLI route.

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

GameSwitcher now derives the same canonical identifier from the proven GB,
GBC, GBA, NES, SNES, and PS launcher mappings. New ROM screenshots use the
GameID digest as their collision-resistant filename. Existing 32-bit FNV path
screenshots remain a read-only fallback, and unknown/custom launchers remain
unidentified instead of receiving a guessed system identity.

Both the structured request and its legacy MainUI adapter use the same durable
metadata sequence: write an exclusive temporary file, flush and close it,
rename it over the destination, then flush the parent directory. Directory
`fsync` reporting `EINVAL` or `ENOTSUP` is accepted for FAT implementations
that do not expose directory flushing; all other write and flush failures are
reported to the caller.

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
The update-state check treats normal idle, staging, validation, and rollback
validation phases as operational. Malformed or unavailable state and terminal
`recovery_required` or `rollback_failed` phases fail health with a bounded
summary that does not expose pending-version or path details.
The save-snapshot check verifies the checksummed inventory and reports only
aggregate total, referenced, and unverified counts. Any corrupt or incomplete
snapshot fails health while remaining preserved as recovery evidence; the
health response does not expose snapshot identifiers or save contents.

`bloomctl logs export` creates a timestamped support archive from an explicit
allowlist: structured info, health, update state, snapshot inventory, bounded
system facts, and Bloom-owned update/test/shutdown logs. It never scans ROMs,
saves, network configuration, SSH material, process environments, or arbitrary
temporary logs. Files are staged beside the final archive and published only
after a non-empty compressed archive has been created.

`bloomctl platform capabilities` exposes the centralized schema-1 platform
inspection as the stable public CLI contract. It reports only observable model,
display, input, battery-backend, and capability data; callers do not need to
repeat hardware heuristics or depend directly on the internal helper.

`bloomctl test smoke` is the stable CLI adapter for the existing developer-mode
game smoke runner. It base64-encodes the ROM path before delegation so spaces
and shell punctuation remain data. The runner retains all safety boundaries:
explicit developer mode, supported-system allowlisting, ROM confinement,
bounded duration, structured launch/session handling, and save flushing.

## Update trust boundary

Release manifests are signed with the BloomOS Ed25519 release key after their
archive digest and size are finalized. Release assembly proves that the private
key supplied by the protected GitHub secret corresponds to the public key
embedded in the image, signs the exact manifest bytes, and verifies the result
before publication. The temporary signing key is removed before the pinned
third-party publishing action runs.

The persistent update channel defaults to `stable` and can be set to `beta` or
`nightly`. Staging enforces a monotonic trust policy: beta may also receive
stable releases, and nightly may receive stable or beta releases, but a selected
channel never accepts a less stable release. Signed `development` artifacts are
accepted only when developer mode is explicitly enabled and cannot be selected
as a normal publication channel.

On-device `bloom-update-verify` verifies that signature before parsing any
manifest fields. It then requires the exact BloomOS schema, a single ZIP
artifact, and matching filename, SHA-256 digest, and byte size. Missing tools,
links, malformed metadata, an unknown key, or any content mismatch fail closed.
This verifier is the required boundary for later online and offline staging;
the inherited size-only OTA path is not trusted as a BloomOS update path.

Verified release inputs are copied into a version-specific directory below
`.bloom/update/staged`. This SD-root state is outside `.tmp_update`, which the
installer replaces during an upgrade. BloomOS verifies the copied bytes a second time,
writes the verified record inside the same temporary directory, flushes pending
writes, and publishes that directory with a single rename. Existing staged
versions are never overwritten. Staging does not extract into or otherwise
modify the live operating-system tree.

Before creating staging state, BloomOS derives the required KiB from the signed
archive size and adds a 16 MiB safety reserve. The current SD free space must
meet that bound; an invalid threshold, unreadable capacity, or insufficient
space fails before any incoming directory or partial payload is created.

The update state machine arms only a freshly re-verified staged payload. Each
unvalidated boot increments a durable attempt counter; the third attempt enters
`recovery_required` instead of continuing indefinitely. A payload becomes
known-good only through an explicit successful-health promotion. Until then,
the prior known-good staged payload and its signed metadata remain addressable
as the recovery source.

The initial installation establishes that chain with `bloomctl update
bootstrap VERSION`. Bootstrap accepts only a signed, staged, prepared release
whose version matches the running system, requires structured health checks,
and refuses to replace an existing known-good record.

Early development builds stored update state under the replaceable
`.tmp_update` runtime tree. On first use, the current state service atomically
adopts that complete legacy directory into `.bloom/update` before reading or
mutating state. It fails closed when both roots exist or either boundary is a
symlink, preserving boot-attempt reconciliation across the upgrade boundary.

Candidate preparation re-verifies the staged archive, rejects links, traversal,
and content outside the `miyoo` and `RetroArch` release roots, then extracts to
an isolated versioned candidate directory. Required installer payloads are
checked before the candidate directory is published. No candidate-preparation
operation writes into the live OS tree.

Activation requires an armed candidate and a retained prior known-good release.
It creates a verified pre-update save snapshot, durably records
`activation_pending`, and publishes package payloads before atomically renaming
the boot installer trigger into place. The guarded operation is exposed as
`bloomctl update activate VERSION` so an offline or future network update
orchestrator can complete the state transition without bypassing these checks.
Existing installer state is never
overwritten. The normal installed MainUI binary is replaced only by the final
atomic shell-trigger rename; an existing shell trigger or symlink fails closed.
The next boot is the first counted validation attempt.

The installed runtime reconciles that durable state immediately after the
installer boundary and before mutable services start. `activation_pending` and
`testing` boots increment the bounded attempt counter; unrelated boots are a
no-op. `bloomctl update confirm` promotes only a `testing` release whose
installed version matches the pending signed version and whose structured
health checks pass. Merely reaching MainUI never marks an update known-good.

After bounded forward-update failures, the early boot reconciler enters
`recovery_required`, automatically publishes the retained rollback payload,
and requests a marked reboot before mutable services start. BloomOS re-verifies
the retained known-good manifest,
signature, archive size, and digest, then replaces any cached extraction with a
fresh isolated candidate. It snapshots current saves before recording
`rollback_pending`, publishes installer payloads before the trigger, and keeps
the original known-good record unchanged. A recovered boot uses the same
version and health confirmation gate. Repeated rollback failures stop in
`rollback_failed` rather than recursively attempting recovery. If publication
or reboot initiation fails, boot fails closed with the durable recovery state
and retained signed payload still available for diagnosis.

`bloom-health-system` supplies the non-database half of the structured health
gate. It fails closed unless platform discovery identifies a supported device
family, required runtime/RetroArch/MainUI payloads exist, the SD card accepts a
mode-restricted write-and-sync probe, and at least 16 MiB remains free. The
probe is removed immediately. `bloomctl health` combines this result with Play
Activity integrity, so update confirmation cannot promote a release that only
passes its database check.

Architectural decisions will be recorded as the corresponding subsystem work begins.
