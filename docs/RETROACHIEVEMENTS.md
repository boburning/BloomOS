# RetroAchievements Platform

## Status and scope

RetroAchievements (RA) is a P0, release-defining BloomOS subsystem. This
document is the architecture contract and ordered implementation plan. No RA
runtime behavior is implemented by the RA-00 architecture milestone.

RA-01 adds only the offline-safe native service skeleton: versioned `status`
and `game` responses, strict Bloom GameID validation, and `bloomctl`
delegation. It deliberately reports `not_configured`, `not_implemented`, and
`unindexed` rather than inventing account, catalog, or RA identity state before
their owning issues land.

RetroArch and rcheevos remain responsible for evaluating and awarding
achievements. Bloom owns content identification, persistent metadata, account
and launch policy, credentials, UI metadata, exact-core certification,
diagnostics, and optional offline transport integration.

The three independent questions are never collapsed:

1. **Game support:** does the exact installed content map to an official RA
   game with at least one achievement?
2. **Core certification:** is the exact shipped core binary reliable for RA and
   Hardcore on Bloom hardware?
3. **Transport:** will this session use direct RA, optional offline proxy, or no
   available network transport?

Bloom GameID remains the canonical identity for activity, saves, screenshots,
and UI history. The RA content hash and RA Game ID are attached metadata and
must never replace it.

## Architecture

```text
Bloom UI (GameSwitcher, settings, future library browser)
                            |
                            v
                        bloom-ra
              +-------------+-------------+
              |             |             |
              v             v             v
       catalog/index   core policy   account/policy
       SQLite+rhash    exact SHA     soft/hardcore
              +-------------+-------------+
                            |
                            v
                   BloomLaunchRequest
                            |
                      bloom-session
                            |
                        RetroArch
                         /     \
                        v       v
                 direct RA   RAOfflineProxy
                 soft/hard   softcore only
```

`bloom-ra` is a native, versioned service boundary. Machine consumers use
schema-1 JSON through `bloomctl achievements ...`; UI consumers never hash ROMs
or perform network requests to decide whether to render a badge.

Initial public commands:

```text
bloomctl achievements status
bloomctl achievements game GAME_ID
bloomctl achievements scan [--changed|--all|--system SYSTEM|--status|--cancel]
bloomctl achievements refresh
bloomctl achievements cores
bloomctl achievements account status
bloomctl achievements proxy status
bloomctl achievements proxy pending
```

## Persistent data

Durable state lives outside `.tmp_update`, under
`/mnt/SDCARD/.bloom/achievements/`. The initial database is
`catalog.sqlite3`. Updates, rollback, MainUI replacement, and proxy restart must
not remove it or any pending proxy awards.

The additive SQLite schema contains:

- `schema_version(version)`;
- `catalog_state(catalog_generation, provider, provider_revision,
  refreshed_at, last_success_at, status)`;
- `ra_games(ra_game_id, ra_console_id, title, official_set,
  achievement_count, metadata_revision)`;
- `ra_hashes(ra_console_id, ra_content_hash, ra_game_id)` with a unique console
  and content-hash key;
- `library_games(bloom_game_id, system_id, normalized_rom_path, file_size,
  file_mtime, ra_console_id, ra_content_hash, ra_game_id, official_set,
  achievement_count, indexed_at, catalog_generation, hash_version, status)`;
- bounded `scan_state` only if durable resumption needs it.

Library status distinguishes `identified`, `unmatched`,
`unsupported_system`, `hash_error`, `deferred`, and `stale`. Newer unsupported
schema versions fail read-only/closed and are never overwritten by rollback.
Wall-clock fields are informational and are not correctness inputs.

## Authoritative hashing and catalog

Bloom source-builds official rcheevos v12.4.0 at commit
`2ad0b8672f68a48148620164510b963039e49eb1` and uses its `rc_hash` interface
for console-specific hashing and its supported archive/disc semantics. CHD
extension detection exists upstream, but Bloom does not claim CHD support until
a compatible reader callback and deterministic fixture pass. Bloom does not invent an MD5 convention or use
filenames as identity. Paths remain structured data, hashing is read-only, and
Bloom rejects traversal, links, and unsafe inputs at its boundary.

An internal `RaCatalogProvider` isolates the scanner and public API from the
current hash-to-game metadata source. It refreshes into staging, validates the
response, builds a new generation transactionally, verifies invariants, and
atomically publishes. Empty, corrupt, or failed refreshes retain the previous
known-good catalog. Launch never depends on refresh success.

The initial provider must be selected only after verifying the current official
or de-facto RA-supported bulk hash/game resolution interface. Provider details
must not leak into GameSwitcher, Bloom GameID, or the public schema.

The initial `ra_web_game_list` provider uses RetroAchievements' documented
`API_GetGameList.php` contract per console with `f=1` (games with achievements)
and `h=1` (supported hashes). The endpoint requires a user's Web API key and
explicitly asks clients to cache aggressively because responses can be large.
Fetch/authentication remains outside the importer until RA-13; RA-04 accepts a
staged response, validates every required field and hash, and publishes it in a
single database transaction. Empty or malformed responses roll back without
replacing the prior known-good generation.

For development and physical validation, `tools/bloom-ra-refresh-installed.ps1`
uses the pinned SSH identity, the device-local login token, and RA's Connect
hash/patch interfaces to resolve only hashes already indexed on the device. It
never prints the token, imports metadata through bounded stdin, and marks this
partial catalog `stale` rather than claiming a complete console catalog. The
bulk Web API provider remains the release refresh path.

## Incremental scanner

The default scanner uses one low-priority hashing worker. It reuses a prior
result when Bloom GameID, normalized path, size, mtime, hash version, system
mapping, and catalog invariants remain valid. It rehashes changed content or an
explicitly forced item.

Scanning is resumable and cancelable, rate-limits large disc reads, pauses when
a game session starts, and never competes with MainUI or GameSwitcher. A full
scan is explicit. Launch may attempt bounded identification of one unindexed
game; failure leaves status unknown and does not block play.

RA-05 implements a single synchronous low-priority worker behind
`scan --changed`, `--all`, and `--system`. Each completed game is committed
independently, so restarting a canceled, paused, or interrupted scan resumes by
skipping durable unchanged rows rather than maintaining a large work queue.
The scanner refuses symlinked roots and entries, ignores artwork/metadata
directories, checks `bloom-session` state between entries, and uses a
recoverable process lock to prevent concurrent scans. Hardware I/O thresholds
and automatic boot scheduling remain deferred until on-device measurement.

## Badge and collections

The normal badge has one exact meaning:

```text
ra_game_id is present
AND official_set is true
AND achievement_count > 0
```

It does not depend on network availability, account state, selected core,
Hardcore, or proxy state. Core certification, transport, cache state, and
pending awards are separate detail fields.

Phase-one badge consumers are GameSwitcher, Play Activity where canonical
GameID is already available, and other Bloom-owned detail surfaces. Closed
MainUI titles and scraped artwork are not modified. GameSwitcher reads a local
API/cache and performs no hashing or network I/O during rendering.

GameSwitcher resolves the canonical GameID once while processing each history
item, queries the durable catalog through a read-only cached SQLite connection,
and stores only the bounded badge/detail fields in its in-memory model. The
render path draws a Bloom-authored generic `RA` glyph from that model and never
opens the catalog, invokes another process, hashes a ROM, or contacts a service.

The `RetroAchievements` smart collection derives from the same index and
contains installed games satisfying the badge rule. No second list or metadata
store is allowed.

`bloomctl achievements collection` exposes the collection as deterministic
schema-1 JSON containing canonical GameIDs and system IDs. It queries
`library_games` directly using the badge predicate, so scanner updates change
membership without regenerating or synchronizing another collection file.

The shipped badge must be either an official mark with documented permission
and pinned provenance, or a reproducible Bloom-authored generic achievement/RA
glyph.

## Status vocabulary

- Game: `supported`, `unsupported`, or `unidentified`.
- Bloom core RA status: `verified`, `best_effort`, `incompatible`, `untested`,
  or `not_applicable`.
- Hardcore status: the same controlled vocabulary.
- Transport: `direct`, `proxy`, or `offline/unavailable`.

## Exact-core certification policy

`build/ra-core-policy.json` records separate upstream and Bloom statuses for
every RA-relevant default system, keyed to the exact core binary SHA-256 in
`build/core-manifest.json`. CI rejects missing default systems, stale hashes,
unknown status values, and any `verified` claim without physical-device test
evidence. A changed core binary therefore invalidates the policy until the new
identity is reviewed and, where applicable, recertified.

Initial upstream classifications were reviewed against the current official
[emulator support](https://docs.retroachievements.org/general/emulator-support-and-issues.html)
and [unsupported cores](https://docs.retroachievements.org/developer-docs/unsupported-emulators-and-cores.html)
references. They remain distinct from Bloom certification: gpSP and
PCSX-ReARMed retain `best_effort` Bloom status pending exact-binary testing,
while no core is marked `verified` without physical evidence.
RA-10 changes the packaged SNES default from Beetle Supafaust to current
Snes9x and the SG-1000 default from Gearsystem to Genesis Plus GX. Both exact
new binaries passed bounded Mini Plus launch, memory, exit, save-flush, and
MainUI-return validation. They remain `untested` for RA behavior until the
achievement, Rich Presence, leaderboard, and Hardcore certification matrix is
completed.
Game Gear, Master System, Caprice32, and FBNeo default decisions remain
explicitly deferred until their performance and ROM-set compatibility can be
measured without weakening existing fallbacks.

## Account and secret ownership

`bloom-ra` owns schema-1 enabled, username, mode, and offline-casual settings
under the durable achievements root. The RA session token is stored in a
separate mode-`0600` credential file; no password is retained. Writes use an
exclusive temporary file, `fsync`, and atomic rename, and symlinked or
group/world-readable credential state is rejected. `bloomctl achievements
account status` reports only configured/enabled/authenticated/mode booleans and
never returns the username or token. Network login/UI wiring will pass an
authenticated token into this boundary without placing secrets in argv.

## Structured launch policy

Schema-1 `BloomLaunchRequest` accepts a strict `achievements` object containing
enabled state, mode, immutable transport, resolved RA Game ID, and exact-core
certification status. Hardcore plus proxy is rejected at validation. Bloom
writes credentials and `cheevos_*` values only to a mode-`0600` append config
under `/tmp/bloom-session/`, then adds that path to the session request. The
permanent RetroArch configuration is never read or modified by this operation.

`bloom-session` invokes a Bloom-owned preparation adapter on its private copy of
the request. The adapter reads the token only from the protected credential file
and passes it to `bloom-launch` through standard input. Unauthenticated or
unidentified Softcore sessions continue with RA disabled; the equivalent
Hardcore uncertainty fails before launch rather than silently downgrading.
Missing exact-core policy is reported as `untested`, while a known incompatible
core disables Softcore RA and rejects Hardcore. Offline Casual requests remain
explicitly unavailable until proxy session routing lands.

RA sessions append only allowlisted launch and finish fields to a mode-`0600`,
size-bounded rotating log under `.bloom/logs`. The log records mode, transport,
core basename and SHA, Bloom certification, outcome, and a bounded reason or
duration. It cannot accept usernames, tokens, game identity, ROM paths, titles,
or arbitrary text. Support export includes only this already-redacted log.

Hardcore policy additionally rejects Bloom auto-resume and proxy transport,
requires Rich Presence and leaderboards, disables rewind/run-ahead/preemptive
frames, and removes load-state, rewind, frame-advance, slowdown, and cheat
hotkeys in the session append config. These controls follow the current
[RetroAchievements Hardcore compliance requirements](https://docs.retroachievements.org/general/hardcore-compliance-requirements.html).
Bloom never silently downgrades a rejected Hardcore request.

- Proxy cache: `not_applicable`, `not_cached`, or `cached`.
- Pending awards: a non-negative aggregate count.

`verified` is a BloomOS claim tied to an exact core SHA-256 and physical
evidence. It is never presented as official upstream RA certification.

## Core policy

`build/ra-core-policy.json` keys each policy entry by system, core basename, and
the exact SHA-256 already recorded in `build/core-manifest.json`. A changed
binary invalidates or downgrades prior certification until it is retested.

Initial performance-first policy:

- Keep gpSP as the GBA default and PCSX-ReARMed as the PS1 default, initially
  `best_effort`; mGBA remains the GBA fallback. Do not replace either default
  solely because upstream does not formally support it.
- Move SNES RA-default behavior from Supafaust to current Snes9x while retaining
  an explicit performance fallback if required.
- Prefer FBNeo for compatible RA Arcade/CPS/Neo Geo sets while preserving MAME
  2003-Plus/FBA fallbacks and treating ROMset compatibility as migration data.
- Prefer Genesis Plus GX for SG-1000 and evaluate it on hardware for Game Gear
  and Master System.
- Keep existing performant defaults elsewhere unless the issue-specific
  certification matrix justifies a change.
- Make no RA release promise for systems listed as experimental or unavailable
  in the tracking issues.

Per-game core overrides are structured launch data. They never change Bloom
GameID or RA game identity and must use Bloom's existing safe core-change/save
handling.

## Account, launch, and Hardcore policy

`bloom-ra` owns enabled state, authenticated account state, mode
(`softcore|hardcore`), and offline-casual preference. Prefer a current RA token
or session credential over long-term plaintext password storage when supported.
Secrets have restrictive permissions and are never emitted by health, exported
logs, session JSON, activity, GameSwitcher, or catalog tables.

The public account preferences remain at
`/mnt/SDCARD/.bloom/achievements/account.json`. The RA token does not: Miyoo's
VFAT SD mount uses `fmask=0000` and cannot enforce private file modes. Bloom
stores the token in device-local JFFS2 at
`/appconfigs/bloom/achievements/credentials` with mode `0600`. Moving an SD
card therefore moves offline metadata and preferences, but never the online
credential; each device must be authenticated separately. The host bootstrap
helper prompts without echo, exchanges the password once for an RA token, and
sends only that token over pinned SSH standard input. Passwords and tokens are
never command-line arguments.

`BloomLaunchRequest` gains a validated achievements policy. Bloom generates a
temporary RetroArch append config; permanent `retroarch.cfg` is byte-identical
before and after every direct or proxy session.

Hardcore is a launch policy, not one config flag. Before launch Bloom disables
automatic/manual state loading, rewind, cheats, and every other prohibited
feature applicable to the pinned RetroArch/rcheevos rules. It never silently
downgrades Hardcore and never routes Hardcore through the proxy. If Bloom
cannot guarantee the requested policy, it reports a bounded error before
launch and requires an explicit alternate choice.

Online readiness queries `bloom-platform` capabilities and distinguishes no
network hardware, disabled/unassociated Wi-Fi, DNS, TLS/time, and RA service
failures. Ordinary play is not blocked by RA failure. Hardcore follows current
RA requirements and is never faked.

## Optional RAOfflineProxy

RAOfflineProxy is a replaceable optional transport added only after hashing,
the local index/badge, core policy, direct RA, and Hardcore work. Its current
Linux behavior supports casual/softcore caching and queued awards; it explicitly
does not support Hardcore.

Bloom packages a pinned, licensed, reproducible upstream source and its runtime
under normal provenance rules. `bloom-ra-proxy` adapts supported upstream CLI
status into schema-1 JSON and does not query upstream-private SQLite or Python
internals. RAOfflineProxy remains authoritative for cached protocol data,
pending awards, deduplication, retry, flush, and anti-tamper behavior.

Session transport is selected before launch and immutable:

```text
Hardcore                         -> direct
Softcore, Offline Casual off    -> direct
Softcore, Offline Casual on     -> proxy
```

The launch resolver rejects Softcore Offline Casual when the proxy is not
ready; it does not silently fall back to direct traffic. Hardcore always
resolves to direct even if Offline Casual is enabled. Once the session append
config has been generated, BloomLaunchRequest rejects any attempt to rewrite
the achievements policy, freezing the chosen transport for that session.

Proxy failure never prevents the emulator or save flush from running. Bloom
uses session append config rather than upstream Onion's permanent-config patch
and revert behavior. If upstream needs a service-only/external-config mode,
Bloom proposes the smallest maintainable upstream contribution before carrying
a local adapter patch.

RA-17 pins upstream `v1.11.1-alpha1` at
`be6898e6dc26338bf82421ff7602fd37932be449`. The service-only package omits
pygame, the SDL menu, and their vendor bundle. It builds the native CHD-aware
hash bridge from the exact rcheevos and libchdr revisions selected upstream,
then combines it with the pinned ARMv7 CPython 3.9.20 runtime. All four archives
have immutable URLs, sizes, and SHA-256 values in
`build/raofflineproxy/sources.json`. The runtime is currently a verified
upstream binary input rather than a Bloom source build, so the package remains
optional and ineligible for a stable public release until that provenance gate
is deliberately resolved. Packaging does not start the proxy or touch any
RetroArch configuration.

`bloom-ra-proxy` is the schema-1 boundary used by the rest of Bloom. Its initial
surface is `status`, `start`, `stop`, `cached-game RA_GAME_ID`, `pending`, and
`health`. An absent optional package is a successful, explicit `not_installed`
state. The adapter accepts only numeric RA game IDs, bounds upstream JSON, and
returns aggregate cache/award state without titles or award details. It starts
upstream's foreground `run-service` operation directly; it never calls
`start-proxy`, `stop-proxy`, or another command that patches or reverts emulator
configuration. `bloomctl achievements proxy status|pending` exposes the two
initial read-only public operations.

The adapter also provides internal `cache-rom PATH` and `cache-system SYSTEM`
operations. Bloom resolves every ROM to a regular, non-symlinked file beneath
the configured ROM root and maps canonical system IDs to their known library
directories before invoking upstream. System batches run in the foreground, so
the caller can cancel them normally; upstream persists each completed cache
entry, making a later retry resumable without a Bloom-owned queue. Favorites,
Recent, all-game selection, progress UI, and rate-limit presentation remain
consumer work on this backend.

## Original Mini and offline behavior

Consumers query network capability instead of hardcoding models. With no usable
network, the local badge, collection, catalog, and core policy remain available;
launch does not wait for impossible connectivity. Direct online/Hardcore state
reports unavailable. A cached casual proxy workflow is experimental until a
Wi-Fi device-to-original-Mini SD-card round trip earns and later flushes an
operator-owned test award successfully.

The database format must permit a future desktop/NAS `bloom-ra-precache` tool
without depending on device-only absolute paths.

## Privacy, health, and failure semantics

`bloomctl health` exposes bounded aggregate state: enabled/mode, account
configured, catalog status/generation/counts, unmatched core-policy count,
proxy installed/running, cached-game count, and pending-award count. It omits
username by default and always omits credentials, ROM paths, hashes, titles,
and individual pending achievements.

RA-24 reconstructs the health object from an explicit allowlist rather than
embedding subsystem output. It carries enabled/state, catalog status and
aggregate counts, plus proxy installed/running/cache/pending counts. Username,
credentials, ROM paths, hashes, titles, award details, and unexpected provider
fields are discarded even if a malformed or incompatible helper prints them.
The support archive consumes only this redacted health result; raw RA/proxy logs
remain excluded until a similarly structured redaction path exists.

Logs are bounded/rotating and may contain subsystem version, catalog generation,
aggregate scan counts, selected transport, core basename/SHA/policy, proxy
state, and generic network error class. Support exports enforce the same
redaction.

Catalog, RA service, proxy, or core-policy failure degrades independently and
does not prevent ordinary game launch. Cached valid badge metadata survives a
temporary catalog failure. Unknown cores report `untested`, never `verified`.
Proxy errors never bypass `bloom-session` or `bloom-save-flush`.

## Validation and release gates

Host CI uses fixtures/mocks and never requires production RA access. Required
coverage includes rcheevos archive/disc/CHD fixtures where supported, malformed
and unsafe inputs, DB migrations/corruption/generation replacement, incremental
skip/invalidation, badge false-positive prevention, exact core-policy SHA
matching, direct/proxy launch matrices, Hardcore enforcement, permanent-config
immutability, secret redaction, proxy adapter failures, and update/rollback.

Physical evidence under `docs/validation/retroachievements/` records Bloom
commit, device/revision, core basename/SHA, RA Game ID, test categories,
Softcore/Hardcore/leaderboard/performance results, date, operator, and notes.
It never contains ROMs, BIOS, credentials, or download locations.

`bloomctl test achievements` is developer-mode guarded. Identification/login,
Rich Presence, Hardcore, lifecycle, and save-flush probes are repeatable;
actual unlocks require an explicit operator mode so tests cannot pollute an
account silently.

The initial RA-25 command performs a privacy-safe certification preflight:
`bloomctl test achievements --system SYSTEM --rom-base64 DATA --core CORE`.
It validates that the decoded ROM belongs to the selected system, derives the
canonical Bloom GameID, queries exact local RA identification, hashes the exact
installed core, and reports the matching Bloom policy status. It never prints
the ROM path or title. Login, Rich Presence, Hardcore, lifecycle, save flush,
and unlock fields remain explicitly `not_run`/`operator_required` until the
physical vertical-slice runner can execute them truthfully; the command does
not fabricate a pass from host evidence.

No core/device is `verified` from host tests. Update and rollback preserve the
catalog and proxy award queue. Stable completion requires the Definition of
Done tracked in the RA-00 epic and child issues.

RA-26 adds a dedicated pull-request/merge-queue gate for all
RetroAchievements-sensitive paths. It revalidates exact core SHAs and evidence
rules, immutable proxy sources, dependency revisions, the required regression
fixture set, service-only proxy integration, transport immutability, Hardcore
direct routing, and session-only custom-host configuration. The focused shell
matrix runs without production RA network access. Native/ASan behavior remains
covered by Bloom's normal test workflow.

## Reference snapshot (2026-08-19)

RA-00 inspected these current primary repositories before implementation:

| Reference | Revision | Material observation |
|---|---|---|
| OnionUI/Onion | `07505ea58c7ba...` | Pinned Bloom baseline and Miyoo compatibility reference. |
| spruceUI/spruceOS | `c7a7c92a2e1d...` | `ra_functions.sh` still centralizes modes but mutates platform config; Bloom retains a native/session-owned boundary. |
| batocera-linux/batocera-emulationstation | `bf0eab201c63...` | `RetroAchievements.*`, `ThreadedHasher.cpp`, and theme bindings retain the exact-content hash-to-game UI pattern. |
| ROCKNIX/distribution | `e044d3c474e4...` | Centralized platform packaging/settings reference; no Bloom credential ownership is delegated to it. |
| RetroAchievements/rcheevos | release `v12.4.0`, commit `2ad0b8672f68...` | MIT-licensed authoritative `rc_hash` implementation pinned and source-built by RA-02. |
| misantronic/RAOfflineProxy | `be6898e6dc26...` | Now GPL-3.0 with Linux/Onion source and current `v1.11.1-alpha1` docs; still casual-only and not a Hardcore transport. This resolves the older no-license observation but not runtime/provenance work. |

Revisions are audit evidence, not automatic dependency selections. Every
shipped dependency receives its own immutable lock, source build, license, and
output identity in its implementation issue.

## Ordered work breakdown

The [RA-00 parent epic](https://github.com/boburning/BloomOS/issues/132) owns
the focused child issues [#134](https://github.com/boburning/BloomOS/issues/134)
through [#160](https://github.com/boburning/BloomOS/issues/160) and this
dependency order:

```text
RA-01 bloom-ra CLI skeleton
RA-02 pinned rcheevos hashing
RA-03 SQLite schema/migrations
RA-04 catalog provider
RA-05 incremental scanner
RA-06 exact-ROM badge metadata
RA-07 GameSwitcher badge + RA-08 smart collection
RA-09 exact-SHA core policy
RA-10 low-risk core-default corrections
RA-11 gpSP + RA-12 PCSX-ReARMed physical certification
RA-13 account/settings
RA-14 structured launch policy
RA-15 Hardcore enforcement
RA-16 direct RA physical vertical slice
RA-17 reproducible optional RAOfflineProxy
RA-18 bloom-ra-proxy adapter
RA-19 upstream proxy integration if required
RA-20 immutable proxy routing
RA-21 cache UX + RA-22 pending-award UX
RA-23 original Mini offline certification
RA-24 health/diagnostics/export
RA-25 smoke/certification tooling
RA-26 complete CI release gate
RA-27 future Bloom library-browser consumer
```

Each issue produces a focused PR, automated tests, contract documentation, and
honest `pending` hardware status. Host-side RAOfflineProxy packaging and adapter
work may proceed while RA-16 physical validation is deferred, but proxy
enablement and public support claims remain blocked until the direct path and
Hardcore invariants are proven on hardware.

## Public terminology

Use “RetroAchievements: Supported/Not detected”, “Core RA status: Verified by
BloomOS/Best effort/Untested/Incompatible”, “Offline achievements:
Cached/Not cached”, and “Offline awards: N pending”. Never call a Bloom result
official upstream certification unless upstream explicitly provides it.
