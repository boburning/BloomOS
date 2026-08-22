# Roadmap

## Implementation status

Last updated: 2026-08-20

This section tracks delivered increments and deferred physical validation. A
checked repository item means the implementation and automated tests have
merged; it does not imply unrecorded hardware certification.

### Completed repository increments

- [x] Preserve the Onion history and immutable upstream baseline.
- [x] Establish BloomOS branding, governance, architecture, compatibility,
  security, development, release, dependency, upstream-audit, Flip-audit, and
  device-support documentation.
- [x] Pin the cross-toolchain, recursive source revisions, theme inputs, and
  GitHub Actions used by current build and test workflows.
- [x] Add BusyBox-oriented shell integration tests and deterministic signed
  release metadata.
- [x] Add the developer-mode harness, key-only SSH for Plus and Flip, host
  deploy/log/smoke tooling, and the guarded original Mini V1-V4 SD-card test
  protocol.
- [x] Centralize current model detection and observable capabilities for
  original Mini, Plus, and Flip.
- [x] Add structured launch requests, canonical GameID foundations, explicit
  session lifecycle, scoped save flushing, and checksummed save snapshots.
- [x] Harden Play Activity identity, duration accounting, migrations, WAL
  operation, health checks, and GameSwitcher identity integration.
- [x] Add clean SD-card shutdown handling that quiesces consumers, disables
  swap, syncs, and remounts FAT read-only before poweroff.
- [x] Add signed offline update verification, immutable staging, isolated
  candidate preparation, durable known-good state, and transactional
  activation.

### Completed hardening increments

- [x] Close the first-update lifecycle gap with signed initial-baseline
  bootstrap, guarded operator activation, newline-safe installed-version
  confirmation, and automatic cleanup of retired Fake-08 and PCSX-ReARMed
  standalones during migration.
- [x] Preserve pending candidates and validation state when upgrading early
  development cards from `.tmp_update/update` to the durable `.bloom/update`
  root.
- [x] Extend the developer-only runtime probe to bounded 15-minute soaks with
  one-second starting and peak RSS telemetry, early-exit detection, graceful
  shutdown, scoped save flush, MainUI return, and command-cleanup gates
  ([PR #122](https://github.com/boburning/BloomOS/pull/122)).
- [x] Reject FAT-incompatible installer payloads before packaging, keep
  source-build workspaces outside the assembled runtime, preserve failed
  extraction diagnostics, and detach the expected live `MainUI` bind mount
  during transactional activation while preserving a stable SSH host identity.
- [x] Explicitly release snapshot operation locks on successful create, list,
  health, prune, and restore paths for BusyBox `ash` compatibility.
- [x] Reconcile activated update boots, count bounded validation attempts, and
  require installed-version plus structured-health confirmation before
  promotion ([PR #54](https://github.com/boburning/BloomOS/pull/54)).
- [x] Reconstruct rollback candidates from retained signed archives, snapshot
  saves, publish the recovery trigger transactionally, and expose the guarded
  `bloomctl update rollback` operation ([PR #55](https://github.com/boburning/BloomOS/pull/55)).
- [x] Automatically publish retained signed known-good media and initiate a
  marked clean reboot when an update reaches its bounded unconfirmed-boot
  limit; harden rollback publication around the live `MainUI` mount and stale
  installer residue.
- [x] Stop modifying permanent emulator launch scripts for temporary reset and
  auto-load-state options ([PR #56](https://github.com/boburning/BloomOS/pull/56)).
- [x] Require known hardware identity, complete runtime payloads, writable SD
  storage, and a free-space floor in structured health and update confirmation
  ([PR #57](https://github.com/boburning/BloomOS/pull/57)).
- [x] Bound verified save snapshots while preserving active recovery references
  and corrupt evidence ([PR #58](https://github.com/boburning/BloomOS/pull/58)).
- [x] Expose read-only structured snapshot recovery inventory through
  `bloomctl` ([PR #59](https://github.com/boburning/BloomOS/pull/59)).
- [x] Retry transient pinned-theme download failures with bounded attempts and
  keep partial responses out of the verified cache ([PR #60](https://github.com/boburning/BloomOS/pull/60)).
- [x] Export a privacy-bounded, allowlisted on-device diagnostics archive
  through `bloomctl logs export` ([PR #61](https://github.com/boburning/BloomOS/pull/61)).
- [x] Expose centralized structured hardware inspection through
  `bloomctl platform capabilities` ([PR #62](https://github.com/boburning/BloomOS/pull/62)).
- [x] Expose the guarded developer-mode game smoke runner through
  `bloomctl test smoke` ([PR #63](https://github.com/boburning/BloomOS/pull/63)).
- [x] Persist stable, beta, and nightly selection and enforce it at the signed
  update staging boundary ([PR #64](https://github.com/boburning/BloomOS/pull/64)).
- [x] Surface malformed and terminal recovery update state through structured
  system health ([PR #65](https://github.com/boburning/BloomOS/pull/65)).
- [x] Verify save-snapshot inventory through aggregate structured health
  without exposing save details ([PR #66](https://github.com/boburning/BloomOS/pull/66)).
- [x] Reject signed update staging before copying when SD capacity cannot hold
  the verified archive plus a safety reserve ([PR #67](https://github.com/boburning/BloomOS/pull/67)).
- [x] Emit and validate exact per-file release payload provenance, including
  unsafe and duplicate path rejection ([PR #68](https://github.com/boburning/BloomOS/pull/68)).
- [x] Remove the unfinished inherited patch workflow that resolves a moving
  upstream latest release ([PR #69](https://github.com/boburning/BloomOS/pull/69)).
- [x] Inventory every shipped libretro core with exact binary identity and
  explicit source, license, patch, compatibility, and validation fields
  ([PR #70](https://github.com/boburning/BloomOS/pull/70)).
- [x] Eliminate the InfoPanel directory double-scan and dynamically grow its
  sorted image list without zero-sized or stale allocations
  ([PR #71](https://github.com/boburning/BloomOS/pull/71)).
- [x] Parse InfoPanel JSON image lists through a compacting, allocation-safe,
  sanitizer-covered module ([PR #72](https://github.com/boburning/BloomOS/pull/72)).
- [x] Require the validation evidence gate before hardware-sensitive claims
  advance ([PR #73](https://github.com/boburning/BloomOS/pull/73)).
- [x] Enforce source, legacy, and excluded provenance tiers at release
  packaging, with legacy payloads restricted to development artifacts
  ([PR #74](https://github.com/boburning/BloomOS/pull/74)).
- [x] Archive and verify the complete shell-test APK closure, rebuild without
  network access, and publish the resulting GHCR image for digest-pinned CI
  ([PR #75](https://github.com/boburning/BloomOS/pull/75)).
- [x] Decompose inherited runtime and package payloads into a canonical,
  tree-hashed component replacement queue enforced during release packaging
  ([PR #76](https://github.com/boburning/BloomOS/pull/76)).
- [x] Attribute unchanged Onion GPL source provenance to script/config-only
  package wrappers without extending that claim to selected emulator binaries
  ([PR #77](https://github.com/boburning/BloomOS/pull/77)).
- [x] Extend the same exact-source attribution to the shared Onion package
  assembly scripts while keeping asset and binary payloads quarantined
  ([PR #78](https://github.com/boburning/BloomOS/pull/78)).
- [x] Resolve the unchanged, binary-free ScummVM launcher package by treating
  its `.miyoocmd` file as a UTF-8 Onion command wrapper
  ([PR #79](https://github.com/boburning/BloomOS/pull/79)).
- [x] Exclude the unreferenced, unattributed FFplay controls sample so the
  functional source-only launcher can leave the legacy provenance tier
  ([PR #80](https://github.com/boburning/BloomOS/pull/80)).
- [x] Preserve the Ports importer and plain-text definitions while excluding
  six bundled example artwork/manual files without recorded licenses
  ([PR #81](https://github.com/boburning/BloomOS/pull/81)).
- [x] Replace Battery Monitor's rights-restricted inherited Arkhip font with a
  checksum-pinned DejaVu Sans release and ship its authoritative license
  ([PR #82](https://github.com/boburning/BloomOS/pull/82)).
- [x] Replace inherited Quick Guide artwork with deterministic Bloom-authored
  pages and enforce byte-for-byte reproduction from repository source
  ([PR #83](https://github.com/boburning/BloomOS/pull/83)).
- [x] Make release provenance evaluate exact legacy-manifest resolutions so
  cleared runtime and package entries no longer leave permanent umbrella
  blockers while unresolved and excluded entries still fail closed
  ([PR #84](https://github.com/boburning/BloomOS/pull/84)).
- [x] Keep AdvanceMENU's functional on-device wrappers while removing bundled
  desktop executables, archives, scripts, and sample media without complete
  component provenance
  ([PR #85](https://github.com/boburning/BloomOS/pull/85)).
- [x] Exclude the redundant Fake-08 standalone package after proving a
  reproducible replacement, retaining PICO-8 support through the existing
  Fake-08 libretro integration.
- [x] Exclude the optional standalone GnGeo package whose exact released source
  was documented as lost, while retaining Neo Geo support through libretro
  ([PR #87](https://github.com/boburning/BloomOS/pull/87)).
- [x] Replace the inherited OpenBOR executable with a deterministic ARM build
  from pinned upstream source, the reviewed Steward-Fu Miyoo patch, and
  source-built codec dependencies; defer device behavior to the hardware matrix
  ([PR #88](https://github.com/boburning/BloomOS/pull/88)).
- [x] Exclude the optional native PICO-8 wrapper whose support payload has no
  declared upstream license, while retaining the Fake-08 libretro option
  and leaving a clean path for a future independently licensed native wrapper
  ([PR #89](https://github.com/boburning/BloomOS/pull/89)).
- [x] Exclude the redundant inherited ScummVM standalone package and retain the
  existing ScummVM libretro integration, avoiding a second engine and codec
  stack while core source-to-binary provenance remains in the shared backlog.
- [x] Exclude the redundant PCSX-ReARMed standalone package, private SDL,
  plugins, and assets while retaining PlayStation support through the existing
  PCSX-ReARMed libretro integration.
- [x] Replace PixelReader and its six inherited private shared libraries with a
  deterministic source build using statically linked, commit-pinned zlib,
  libxml2, and libzip; retain the pinned upstream DejaVu fonts and use Bloom's
  shared SDL runtime.
- [x] Keep the inherited provenance inventory immutable during release builds
  and route the OpenBOR and PixelReader replacements directly
  into package staging so the signed payload contains the rebuilt artifacts.
- [x] Restrict signed OTA archives to the updater-supported `miyoo/` and
  `RetroArch/` roots, keeping future first-install media as a separate artifact.
- [x] Exclude the optional 240pSuite shortcut rather than ship an inherited ROM
  and dedicated SNES core without exact source-to-binary evidence. Upstream's
  GPL source remains a candidate once its complete host toolchain can be rebuilt
  from attributable source; Bloom retains normal SNES support meanwhile.
- [x] Exclude DinguxCommander and its package because the upstream repository
  has no declared project license; require an independently implemented or
  authoritatively licensed file manager before restoring the feature.
- [x] Exclude the inherited GMU package, whose GPL application is bundled with
  an incompletely mapped frontend, decoder/codec stack, themes, and sample
  music; require an all-source, license-complete replacement before restoring
  music playback.
- [x] Exclude the inherited proprietary DraStic package and its incompletely
  attributable libraries, firmware, databases, fonts, and interface assets;
  remove the now-invalid legacy migration scripts, reserve the `NDS` library
  path, remove the retired package from upgraded cards without touching ROMs
  or saves, and require a reproducible, licensed, device-qualified emulator
  before Nintendo DS support returns.
- [x] Exclude the Search/Filter source and package integration because the
  upstream project has no authoritative license; retain per-system ROM cache
  refresh through Bloom's licensed shell implementation and require a clean-room
  or authoritatively licensed replacement before global search returns.
- [x] Replace the shared application-library bundle with deterministic builds
  of SQLite 3.39.0 and SDL_rotozoom from repository source; remove unused
  `libgfx` and the unused unlicensed keyboard wrapper, and rely on the Miyoo
  runtime layer instead of duplicating its `libpng` and `libshmvar` binaries.
- [x] Stop redistributing 44 byte-identical Miyoo firmware libraries and one
  obsolete zlib copy in the SD-card runtime; load stock libraries from
  `/customer/lib` while retaining Bloom's generated `libgamename` override and
  the audio preload shim required by existing launch commands.
- [x] Split the final inherited `.tmp_update` and `miyoo` runtime trees into
  independently auditable payload units, resolve reviewed repository source,
  and remove the stale static `libgamename.so` that release builds overwrite
  ([#127](https://github.com/boburning/BloomOS/pull/127)).
- [x] Decompose the inherited runtime executable bundle, attribute Bloom-owned
  command tools independently, and remove the stale checked-in Dropbear output
  that the pinned release build regenerates
  ([#128](https://github.com/boburning/BloomOS/pull/128)).
- [x] Decompose inherited runtime scripts by command or subsystem, resolve the
  exact Onion source revision for reviewed text, and isolate the mixed scraper
  databases as a fail-closed payload
  ([#129](https://github.com/boburning/BloomOS/pull/129)).
- [x] Decompose the inherited runtime library bundle into independently
  replaceable libraries and runtime stacks without inferring provenance from
  binary names ([#130](https://github.com/boburning/BloomOS/pull/130)).
- [x] Decompose runtime resources, resolve the release public key and reviewed
  text configuration, and isolate inherited artwork for reproducible
  replacement ([#131](https://github.com/boburning/BloomOS/pull/131)).
- [x] Retire the inherited parallel Dropbear/SFTP binaries and the legacy SSH
  password/no-password toggles; retain only Bloom's source-built, key-only SSH
  service behind explicit developer mode and a provisioned public key.

The hardware-sensitive BloomOS 1.0/1.1 implementation queue is at its
physical-validation boundary. Source-provenance replacement work can continue
independently, but the checks below must produce device evidence before
hardware-sensitive behavior or public stable-release claims advance.

### Deferred external provenance

Stable publication is fail-closed through `build/provenance-policy.json`.
Inherited components without authoritative source, build, and license evidence
are quarantined to development artifacts until they are replaced by reviewed
source builds. The shell-test APK closure is archived and checksum-locked, and
normal CI consumes its offline-built OCI image by immutable digest. Exact
release and libretro binary identities are locked. The broader runtime and
package backlog is decomposed in `build/legacy-manifest.json` so components can
be independently rebuilt or excluded; missing historical evidence will not be
guessed from inherited bytes.

### Deferred physical validation

- [x] Inspect and power on the Miyoo Mini Plus after the 2026-08-17 remote
  reboot command completed shutdown but did not return to Wi-Fi; the recovered
  persistent log proves swapoff, read-only FAT remount, and recursive unmount
  completed before the device required manual power-on
  ([PR #90](https://github.com/boburning/BloomOS/pull/90)).
- [x] Diagnose the Plus reboot/power-control handoff and add a delayed direct
  kernel-reboot fallback without changing the proven clean SD-card quiescing
  path ([PR #91](https://github.com/boburning/BloomOS/pull/91)).
- [x] Remove the unnecessary BusyBox `su -c` detach layer and replace the
  positional reboot-mode handoff with a validated `/tmp` state file, eliminating
  ambiguity exposed by the first instrumented Plus runs ([PR #94](https://github.com/boburning/BloomOS/pull/94)).
- [x] Validate clean reboot on an unplugged Plus: the device returned to SSH
  automatically, logged `shutdown_mode=reboot`, completed the read-only remount
  and recursive unmount, and accepted the normal init reboot command.
- [x] Validate the one-boot `reboot-to-system` handoff on a USB-powered Plus.
  Signed build `65a9384a` returned to SSH automatically in under one minute
  without a physical power press. Persistent telemetry recorded the reboot
  marker, swapoff, successful read-only FAT remount, recursive unmount, and
  accepted init reboot; fresh userspace consumed the marker before charging
  mode. Two candidate boots passed structured health with five verified save
  snapshots, and `65a9384a` was promoted over known-good `142c2922`.
- [x] Activate signed build `142c2922` over retained known-good `c1ccb004` on
  Plus, boot through the real installer, confirm the exact installed version,
  pass consecutive structured health checks, preserve four verified save
  snapshots and both NDS ROMs, remove the retired DraStic package, and promote
  the candidate to known-good.
- [x] Validate bounded automatic rollback on Plus with signed candidate
  `4210275f` over retained known-good `65a9384a`. Deliberately leave two healthy
  candidate boots unconfirmed, then verify that the third attempt publishes the
  retained signed payload and reboots without physical input. The recovered
  boot reported `operation: rollback`, preserved verified pre-update and
  pre-rollback snapshots, passed structured health, and confirmed `65a9384a`
  with `recovered_from: 4210275f`.
- [ ] Repeat the applicable reboot/poweroff paths on Mini V2 and Flip. Confirm
  that ordinary USB cable insertion still enters charging-only mode while an
  explicit reboot bypasses it exactly once on each applicable model.
- [ ] Repeat applicable signed update activation, boot confirmation,
  bounded-failure, and automatic rollback coverage on Mini V2 and Flip.
- [x] Run remote Plus lifecycle probes for GB, GBC, GBA, NES, SNES, and PSX for
  60 seconds each, then run PCSX-ReARMed for 900 seconds with continuous RSS
  sampling. All cores exited through RetroArch `QUIT`, flushed saves, returned
  to MainUI, and removed launch state. PCSX-ReARMed peaked at 37,856 KiB,
  preserved its 128 KiB SRAM/memory-card file, atomically refreshed its
  automatic state, and left structured health green on signed build `6b319e30`.
- [x] Add guarded Fake-08 probing ([PR #124](https://github.com/boburning/BloomOS/pull/124))
  and validate an original device-local PICO-8 cartridge on Plus build
  `71080921`. A 60-second run and a repeated 300-second run stayed alive,
  peaked at 11,464 and 13,572 KiB respectively, flushed saves, returned to
  MainUI, removed launch state, and created/refreshed automatic state and
  screenshot files. The repeat exited through RetroArch `QUIT` and left
  structured health green.
- [ ] Complete visual/audio/input/SRAM/save-state and longer soak coverage on
  maintainer-owned V2, Plus, and Flip hardware.
- [ ] Validate Battery Monitor text fit and readability on V2, Plus, and Flip
  after replacing Arkhip with DejaVu Sans 2.37.
- [ ] Validate Quick Guide page readability and shortcut accuracy on V2, Plus,
  and Flip after replacing the inherited artwork.
- [ ] Validate Fake-08 libretro startup, audio, input, save behavior, and
  representative cartridge compatibility on V2, Plus, and Flip.
- [ ] Validate source-built OpenBOR startup, rendering, audio, input mappings,
  PAK loading, and save behavior on V2, Plus, and Flip.
- [ ] Validate PCSX-ReARMed libretro startup, rendering, audio, input, memory
  cards, save states, and representative games on V2, Plus, and Flip.
- [ ] Collect equivalent external physical evidence for original Mini V1, V3,
  and V4 before claiming stable support for those revisions.

## BloomOS 1.0 productization

The original roadmap treated a replacement frontend as later research. Bloom's
structured launch, session, save, activity, update, platform, health, and
RetroAchievements boundaries are now mature enough to move frontend ownership
into 1.0 safely. Historical rationale remains below; the authoritative product,
UX, design, migration, and validation contracts are:

- [1.0 product specification](1.0_PRODUCT_SPEC.md)
- [1.0 UX contract](UX_1.0.md)
- [branding and visual design](BRANDING_AND_VISUAL_DESIGN.md)
- [MainUI migration map](MAINUI_MIGRATION.md)
- [1.0 test matrix](1.0_TEST_MATRIX.md)

The implementation order is: inventory current MainUI responsibilities and
user-facing Onion assets; establish Bloom design primitives and single settings
and library authorities; deliver a Bloom Shell vertical slice; migrate primary
navigation, GameSwitcher, settings, health, updates, apps, and themes; add Safe
Mode; decouple health, boot, update, and game return from MainUI; then complete
performance, migration, physical hardware, and release-candidate gates.

Bloom Shell is the normal runtime frontend. MainUI is not a stable 1.0 frontend
and is reachable only through the explicit Developer Mode recovery gate while
remaining physical certification is completed.

Safe Mode is implemented behind the development shell. Each start has a durable
bounded launch ID and phase; an interrupted or early failed start increments a
consecutive counter, a sustained live shell clears the counter, and three
failures latch Safe Mode. The state service refuses malformed or symlinked data
and publishes atomically. Runtime suppresses automatic resume and custom
startup scripts, disables RetroAchievements session work, and relaunches into
one Bloom-owned recovery list instead of expanding the MainUI fallback. Games
remain browsable, while health refresh, support export, confirmed signed
rollback, confirmed Settings reset, and explicit normal restart use fixed
service boundaries. Settings reset preserves a private exact backup before
atomically publishing and materializing defaults; game, save, state, library,
history, and credential data remain outside its boundary. The single-screen
first-run presentation now reconciles preserved settings before atomically
publishing a private completion marker, remains retryable after interruption,
and bypasses safely in recovery. Bloom Shell is now the unflagged runtime
default; only the conjunction of regular Developer Mode and explicit MainUI
fallback markers selects the retained recovery frontend. A signed Plus update
booted this default, completed first run, reached the root with MainUI absent,
and was promoted to known-good with aggregate health green. Physical boot-loop
validation and the remaining device matrix still precede removal of the
development fallback from the payload.

Current frontend progress:

- [x] Shared resolution-safe navigation, focus, dialog, and text-entry state.
- [x] Shared Bloom SDL rendering and device-input foundation. Deterministic
  shell geometry, host golden coverage, and semantic Miyoo key mapping landed
  in #238 and #239; physical display/input claims remain tracked by #228.
- [x] Bloom settings authority. Schema-1 durable state, exact legacy snapshot,
  idempotent Onion import, serialized legacy-authority synchronization, and a
  complete durable keymon/Tweaks settings model are implemented. Boot, MainUI
  shared-memory changes, and WPS preference changes now reconcile the inactive
  canonical copy. Typed canonical reads, fail-closed Onion compatibility
  materialization, allowlisted direct mutations, and guarded activation/rollback
  are implemented. Signed build `9b5c3aa8` passed Plus activation, mutation,
  closed-MainUI compatibility reconciliation, rollback, reactivation, automatic
  reboot persistence, FAT read-only remount, and structured health checks;
  Bloom is the active authority on that device. Closed-MainUI writes continue
  through the service-owned reconciliation bridge until that UI is replaced.
- [x] Bloom library authority. The durable transactional SQLite schema and
  bounded status boundary are implemented. Deterministic Onion system/app
  import is implemented behind a signed 53-system mapping with transactional
  publication, repeat no-op behavior, and prior-known-good preservation.
  Incremental game enumeration/invalidation and bounded GameID-cursor paging
  are implemented; a 10k-game host fixture completes in about 1.1 seconds, and
  an exact-hash Plus candidate indexed 22 installed games with a repeat no-op.
  Deterministic read-only Onion favorite/recent import is implemented with
  canonical GameID matching and explicit unmatched, duplicate, and invalid
  outcomes. An exact-hash Plus candidate matched a real indexed favorite,
  classified the remaining fixture and app-only recent entries correctly, and
  repeated idempotently before restoring the database and MainUI lists to their
  original hashes. Signed build `4093ec3e` physically proved Bloom-owned Shell,
  launch, Recent, GameSwitcher, and Continue consumers plus durable persistence;
  the remaining device-matrix work is tracked under hardware and UX certification.
- [ ] Bloom power and network ownership. The first `bloom-power` boundary is
  implemented with model-aware status, fixed reboot/poweroff requests, and
  fail-closed unknown hardware. Runtime shutdown and automatic update rollback
  use it while retaining the proven low-level FAT-clean shutdown implementation;
  keymon and charging state now use the same adapter. The read-only
  `bloom-network` boundary now separates capability, enabled state, and local
  association without exposing network identity; the RA readiness probe
  consumes it before its bounded internet/TLS check. Exact-hash Plus evidence
  confirms the real firmware setting reader and sysfs association path. Normal
  runtime reconciliation now uses a single fixed Bloom operation; an exact-hash
  Plus candidate preserved association, the Wi-Fi/DHCP processes, and SSH while
  original Mini is an explicit no-network no-op. Bounded enable/disable now
  persists only the canonical Bloom Wi-Fi preference before applying that
  fixed backend, with separate settings/apply failures; a no-op Plus enable
  preserved the exact settings hash and association.
  `bloom-controls` now owns bounded canonical brightness persistence and the
  shared logical-to-PWM apply curve; boot uses the non-persisting adapter path,
  and an exact-hash Plus no-op retained both raw PWM and settings bytes.
  The same boundary now owns 0-20 volume persistence and a source-built,
  nonblocking Plus audio-server FIFO backend. Exact-hash evidence confirms an
  idempotent -60 dB request left settings byte-identical and the audio server
  alive; keymon uses the fallback for normal adjustment and resume restoration.
  `bloom-time` now owns RTC/clock status and validated boot reconciliation;
  exact-hash Plus evidence took the no-mutation RTC path while no-RTC fallback
  and failure semantics are host-tested.
  `bloom-lid status` now normalizes both observed Flip hall-sensor paths and
  fails closed for missing, linked, or malformed state. Plus exact-hash
  evidence confirms the explicit unsupported path without mutation; Flip
  suspend/resume policy remains deferred for physical validation.
  `bloom-wifi` now owns the fixed Plus radio, WPA, DHCP, and power-save
  lifecycle while the inherited network script is restricted to auxiliary
  compatibility services. Exact-hash Plus evidence confirms an associated
  reconciliation is a true no-op that preserves settings bytes, network-client
  process IDs, carrier, and SSH. Suspend mutation, non-Plus public volume
  backends, Flip Wi-Fi lifecycle ownership, and replacement of the remaining
  inherited auxiliary network services remain pending.
- [x] Bloom Shell vertical slice and explicit Developer MainUI fallback. The native
  shell renders one Continue plus Games/Favorites/Recent/Apps/Settings root
  from bounded in-memory catalog rows without reparsing MainUI state. Library,
  favorite, recent, and Continue selections stage through the structured launch
  and session services and return through the runtime loop. The reviewed Plus
  PICO-8 and ScummVM wrappers are RetroArch-backed, so their systems use the
  existing structured core/session/save boundary; arbitrary Ports remain hidden
  pending a standalone durability contract. Apps are classified explicitly; only
  `bloom-native` and reviewed `onion-compatible` launchers can cross the
  supervised, atomic `App/` command boundary, while MainUI-dependent and
  development-only rows are refused. A signed Plus launch found that the legacy
  Activity Tracker exits with status 139; Bloom recovered directly without
  MainUI and remained healthy. The package is therefore no longer exposed as
  stable; its carried declaration explicitly revokes the prior classification
  without deleting rollback-compatible upgrade files. Richer activity UX stays
  assigned to Bloom-owned Recent/game detail. Render-time
  subprocesses are prohibited by a shell gate. Developer Mode exposes only the
  separately classified development rows. The shared renderer now provides a
  bounded one-to-three-button confirmation layer with safe focus and distinct destructive treatment,
  ready for guarded update actions without exposing those mutations yet. The
  same shared layer now renders the bounded on-screen keyboard modes needed by
  account entry while keeping credential text and glyph drawing outside the
  renderer. RetroAchievements Settings now opens that keyboard for bounded
  username and token entry, masks the token, submits it through fixed arguments
  and standard input, clears it immediately, and reports success or failure.
  Signed-in accounts use the shared safe-default confirmation dialog before a
  fixed-argument sign-out request can remove credentials.
  Bloom Shell is the normal unflagged frontend. Repeated early exits retry the
  shell through the guarded crash threshold and enter Bloom Safe Mode; MainUI
  requires both Developer Mode and a separate explicit fallback marker. Signed
  Plus evidence covers the default boot, recoverable first run, direct root,
  system browsing, supervised app launch/return, one real structured GB launch,
  canonical Recent/GameSwitcher/Continue propagation, and the Bloom-owned
  recovery surface with MainUI absent. Physical controls, semantic save/state
  behavior, representative multi-system play, and boot-loop validation remain
  under the hardware/default-shell gates.
  A separate 30-minute signed Plus game soak held one RetroArch PID, 10,804 KiB
  RSS, 56,616 KiB VmSize, and 13 file descriptors constant across 61 samples;
  cleanup completed the scoped save flush and retained green aggregate health.
- [x] Stable theme isolation. Bloom-owned surfaces use the fixed Bloom design
  system and never invoke the legacy `themeSwitcher`; missing, malformed, and
  MainUI-specific theme state therefore cannot redirect stable shell styling.
  Legacy reapply remains only inside the explicit two-marker Developer MainUI
  fallback. Auxiliary filebrowser branding is BloomOS. Host regression covers
  malformed state and gate combinations; signed deployment and physical boot
  appearance remain pending.
- [x] Unified Bloom Settings and Quick Settings. Bloom Shell exposes one flat,
  sectioned Settings surface with capability-filtered Network and Developer
  rows, focused overlays only where needed, and a bounded START Quick Settings
  model on every normal shell screen. It loads the bounded canonical values
  once before rendering, shows actual brightness/volume/mute/Wi-Fi state, and
  routes left/right changes through fixed Bloom control and network adapters;
  failed reads are explicit and failed requests do not change the displayed
  value. Battery comes from a separate bounded platform adapter that
  normalizes sysfs, batmon, and AXP paths without moving hardware heuristics
  into the shell. Display, Audio, and Network reuse the fixed adapters; Update
  uses its focused confirmation overlay. RetroAchievements exposes inline enable,
  Softcore/Hardcore, and offline-award
  controls plus one focused account overlay that returns directly to Settings.
  Audio mute is actionable through the canonical control adapter and restores
  the persisted volume when unmuted.
  System shows bounded free storage from the existing health probe, and System
  Health offers a fixed, timeout-bounded support export with explicit completion
  or failure feedback. Guarded update actions, RetroAchievements account
  interaction, reusable keyboard flows, and GameSwitcher's explicit Open Settings
  action are implemented; physical readability/input and adjustment-latency
  validation remain pending. The bounded `settings values` response exposes only
  the four high-frequency device controls plus
  generation/authority, without leaking unrelated canonical or legacy state.

## Historical BloomOS foundation

1. Repository bootstrap, attribution, upstream and Flip audits, and dependency inventory.
2. Pinned build environment, CI, host and shell tests, reproducible packaging, and developer harness.
3. One-build baseline across Mini V1–V4, Plus, and Flip, with maintainer validation on V2/Plus/Flip and recorded external evidence for revisions not locally owned.
4. High-confidence correctness fixes and a capability-based platform abstraction.
5. Structured launch and session lifecycle, canonical game identity, activity correctness, and save safety.
6. Regression hardening, physical test matrix, migration safety, recovery, and release tooling.

BloomOS 1.0 is not blocked on Sync, Hub, Link, or profiles. A Bloom-owned
frontend replacing MainUI in normal operation is now required for 1.0.

## Later 1.x releases

Planned work includes safe updates and rollback, optional synchronization and offline achievements, a package hub, profiles and Kids Mode, system health, performance profiles, Nearby Play, a local browser companion, and a unified in-game experience.

### RetroAchievements P0 platform

- [x] RA-10: switch the packaged SNES default to current Snes9x and SG-1000 to
  Genesis Plus GX while retaining the inherited fallback packages. Exact-core
  Mini Plus sessions passed bounded memory, clean-exit, save-flush, MainUI
  return, and command-cleanup checks. RA feature certification remains separate
  and no `verified` claim is inferred from these lifecycle tests.
- [ ] RA-02: finish authoritative rcheevos hashing coverage. Raw and bounded
  cartridge ZIP hashing are implemented; physical Mini Plus validation maps
  known GBA and SNES fixtures to their real RA Game IDs. The source-pinned
  CHD/PBP bridge produces the authoritative hash for a real operator-owned CHD
  on both the host and Mini Plus, with matching evidence. A real two-disc PS1
  playlist now validates upstream first-disc semantics on the host and Plus
  while rejecting unconfined first entries. Schema-v2 dependency signals also
  skip unchanged playlists without trusting the `.m3u` file alone. Guarded PBP
  and remaining archive-boundary fixtures are still pending.
- [ ] RA-05: finish the incremental scanner. Changed scans persist completed
  rows for safe restart, use one low-priority worker, and now recheck cancel and
  active-session signals before every directory entry so large flat libraries
  yield promptly to gameplay. Session preparation also makes one bounded,
  exact-GameID identification attempt for an unindexed launch. Durable
  scan-position reporting remains pending.
- [ ] RA-04: finish the production catalog fetch path. Transactional full-console
  import and a secure installed-game development refresh bridge are implemented;
  Mini Plus validation persists ten exact RA matches offline. The bridge now
  excludes Connect achievements not flagged as published/core. Boktai exposed
  a remaining provider discrepancy (68 published Connect rows versus the 67
  achievements shown by the live client and official game page), so exact
  public achievement counts remain a production-provider release gate.
  On-device TLS or authenticated bulk Web API acquisition remains pending.

- [ ] Deliver the ordered RA-00 through RA-26 implementation in
  [`RETROACHIEVEMENTS.md`](RETROACHIEVEMENTS.md): exact rcheevos identification,
  persistent offline metadata, GameSwitcher badge and smart collection,
  exact-SHA Bloom core policy, centrally owned account and temporary launch
  configuration, enforced direct Hardcore, optional softcore-only
  RAOfflineProxy, redacted diagnostics, CI, and physical certification
  ([RA-00 epic #132](https://github.com/boburning/BloomOS/issues/132)).
- [ ] RA-17: package the pinned RAOfflineProxy service backend and native hash
  bridge reproducibly. Source/runtime locks and fail-closed verification are
  implemented. Fresh ARM builds are byte-identical; an initial physical install
  exposed forbidden runtime symlinks on FAT, so the recipe now prunes
  developer-only payloads, fails closed unless the result is symlink-free, and
  carries the pinned toolchain's missing `libutil.so.1` runtime companion.
  The current 18,520,645-byte package reproduces exactly across distinct build
  directories, uses its pinned runtime CA bundle, and physically runs on Mini
  Plus. The optional runtime remains outside stable releases until its
  binary-input provenance is admitted or replaced by a source build.
- [x] RA-13: account/settings UI and authentication lifecycle. The
  canonical service, redacted status, hidden host login bootstrap, one-time
  password-to-token exchange, and device-local mode-`0600` JFFS2 token storage
  are implemented and physically exercised with both a disposable token and a
  real RA login on Mini Plus. Redacted account status, enable/mode/offline
  settings mutations, and sign-out are available through the stable CLI. Bloom's
  flat Settings surface now owns graphical account status, inline
  enable/mode/offline controls, device-native QWERTY credential entry, sign-out,
  and connection status. The earlier Tweaks implementation supplied physical UI
  validation and a real password-to-token login pass on Mini Plus; the Bloom-owned
  replacement still requires physical UX validation. Passwords
  remain masked, stdin-only, and are not retained; only the device-local token
  is stored.
- [ ] RA-14: finish session-runner integration for structured RA launch policy.
  `BloomLaunchRequest`, immutable transport resolution, and mode-`0600`
  temporary append configs are implemented; the launch CLI now accepts tokens
  only through stdin. The normal `bloom-session` path now resolves canonical
  account, exact-game, and exact-core policy into a private request copy and
  session-only config. Mini Plus preparation validated exact Game ID 568,
  best-effort gpSP policy, mode-`0600` config creation, credential presence,
  and byte-identical permanent RetroArch config. A 60-second physical direct
  Softcore session then proved the config reached RetroArch, graceful exit,
  save flush, MainUI return, and removal of both private config and temporary
  launcher. Observable live login and a real Softcore unlock now pass; Rich
  Presence remains pending.
- [ ] RA-15: finish Hardcore enforcement and certification. Host policy rejects
  proxy transport and silent downgrade. A 60-second Mini Plus session proved
  direct transport plus runtime disabling of auto-load, rewind, run-ahead,
  preemptive frames, load-state, and cheat controls; it also completed graceful
  exit/save flush/cleanup and restored the account to Softcore. Server-side
  validation subsequently returned RetroAchievements' unknown-emulator warning
  for the exact gpSP binary, so policy now marks its Hardcore support as
  unsupported. Bloom instead selects the exact mGBA fallback in the private
  request. A 60-second physical mGBA Hardcore session passed direct routing,
  restrictions, graceful exit, scoped save flush, cleanup, MainUI return, and
  Softcore account restoration. A real Hardcore unlock and other core/device
  certification remain pending.
- [ ] RA-16: complete the direct RA vertical slice. Mini Plus now physically
  proves token login, exact Game ID 568 recognition, continued gameplay,
  graceful exit, save flush, and MainUI return through the real direct path.
  A real two-point Softcore achievement was evaluated and awarded through gpSP,
  persisted after graceful exit/save flush, and appeared in a fresh authenticated
  server session. Rich Presence remains pending.
- [ ] RA-18: ship the schema-1 `bloom-ra-proxy` adapter. Host coverage now
  proves absent-package degradation, bounded status/pending/cache translation,
  input rejection, and stop behavior without config mutation. Physical package
  startup exposed that pinned upstream `service-status` is human-readable, so
  lifecycle control now consumes its bounded `home-status` JSON contract.
  Corrected Mini Plus status/start/pending/stop lifecycle passes with zero
  pending awards and a clean stopped final state.
- [ ] RA-20: resolve direct/proxy transport before launch and freeze it after
  session append-config generation. Host tests cover direct Softcore, proxy
  Softcore, unavailable-proxy rejection, forced-direct Hardcore, and permanent
  config immutability. Physical direct Softcore and Hardcore routing pass. Mini
  Plus validation now also proves pre-launch proxy startup, a fixed loopback
  endpoint, immutable proxy session state, mode-`0600` append config, unchanged
  permanent RetroArch config, and clean account/proxy restoration. A real cached
  offline award remains part of RA-21/RA-23 validation.
- [x] RA-21: offline-cache UX. The adapter safely supports resumable,
  foreground per-ROM and per-system caching with ROM-root confinement. The
  public CLI now exposes both operations. A private credential bridge imports
  Bloom's canonical token without command-line/log output or permanent
  RetroArch mutation, and removes its mode-`0600` temporary config after use.
  Mini Plus physically cached RA Game ID 8038 over verified TLS and reported it
  through the adapter. Favorites, Recent, and all supported-system selectors
  now run serially with resumable upstream state and privacy-bounded aggregate
  results. The legacy Tweaks UI supplied evidence for bounded progress, `B`
  cancellation of only the cache process group, and aggregate completion/error
  counts. Its stable Bloom-owned contextual or Settings-detail replacement remains
  migration work rather than a reason to expose Tweaks.
  Mini Plus physical cancellation confirmed that no cache worker or temporary
  credential remained, the proxy stayed stopped, the account stayed
  authenticated, and permanent RetroArch configuration stayed unchanged.
- [ ] RA-22: add pending-award UX. The public aggregate command now separates
  clear, pending, waiting-for-network, and authentication-required states while
  exposing only pending count and bounded transport booleans. Bloom does not
  parse upstream's database or claim a last-flush result that upstream does not
  expose. A graphical settings/detail surface and a real queued-award flush
  remain pending.
- [x] RA-24: aggregate RA health and support-export diagnostics. Health
  allowlists catalog/proxy counts and rejects or discards secrets, ROM paths,
  titles, award details, and unexpected fields. Session launch/finish diagnostics
  now use a mode-`0600`, size-bounded rotating allowlist and support export may
  include that redacted log. Native status reports live redacted account,
  catalog, and index state. Mini Plus validation confirmed two structured
  session events, enforced JFFS2 mode `0600`, forbidden-data absence, and an
  identical allowlisted support-export copy. Preparation failures now emit only
  bounded categories for account, request, core-policy, Hardcore, proxy, and
  config-generation failures. An explicit bounded readiness probe now separates
  absent network hardware, Wi-Fi state, association, clock, DNS, TLS, timeout,
  and RA-service failures without adding network I/O to ordinary health
  rendering. Mini Plus runtime injection physically confirmed every transport
  failure category and a healthy real endpoint without changing Wi-Fi state.
  Support export now reconstructs only valid schema-1 RA log events, drops
  malformed lines and finish details, and physically passed a forbidden-data
  archive scan.
- [x] RA-25: add guarded RA certification tooling. Developer-mode preflight now
  verifies exact ROM identity and installed core SHA/policy without exposing
  ROM or account data and reports the current redacted authentication state;
  bounded default-core sessions now automate graceful lifecycle and save-flush
  validation. Certification results now report the exact post-policy core and
  SHA used by the session, so a GBA Hardcore request records mGBA rather than
  the requested gpSP default. Mini Plus preflight and a 10-second gpSP session
  pass. Ordinary automated sessions now force RA off and physically add no RA
  log events. Live RA is available only behind the explicit
  `I_ACCEPT_PROFILE_CHANGES` operator mode; a physical Plus run produced the
  expected direct launch/finish pair, exited cleanly, flushed saves, restored
  MainUI, and removed its temporary config. A prior physical gpSP Softcore
  unlock and post-session server verification also pass. Rich Presence remains
  an explicit operator observation for the remaining vertical-slice matrix.
- [x] RA-26: enforce release-sensitive regression contracts inside the complete
  shell gate for exact core/source policy, required RA fixtures, service-only
  proxy integration, immutable transport, Hardcore routing, redaction-sensitive
  behavior, and focused offline tests without production RA network access.
  Pull-request scheduling now runs one shell gate for shell-only changes, limits
  native/ARM jobs to compile-relevant paths, cancels superseded runs, and caches
  exact pinned submodule objects with bounded checkout retries.
- [x] RA-27 consumes the existing local RA-06 metadata in the Bloom-owned
  primary browser. Bloom Shell loads exact supported GameIDs once before its
  render loop and displays a compact RA badge in the shared list and preview.
  Missing or malformed optional metadata degrades to no badge without blocking
  browsing; no ROM hashing, network access, duplicate authority, or per-frame
  database work was added.

- [ ] Restore Nintendo DS support with a reproducible, open-source libretro
  core such as melonDS or DeSmuME. Build it from pinned source for Bloom's
  ARMv7 runtime, keep proprietary BIOS/firmware out of releases, and qualify
  startup, rendering, audio, input, screen layouts, touch-pointer controls,
  saves, and representative-game performance before enabling `NDS` on Mini
  V1-V4, Plus, or Flip. DraStic and other proprietary standalone packages are
  not candidates for BloomOS distribution.

## BloomOS 2.0 research

An open MainUI replacement may be prototyped only after stable backend APIs and BloomOS 1.x compatibility are established.
