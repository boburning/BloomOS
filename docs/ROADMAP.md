# Roadmap

## Implementation status

Last updated: 2026-08-19

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

## BloomOS 1.0 foundation

1. Repository bootstrap, attribution, upstream and Flip audits, and dependency inventory.
2. Pinned build environment, CI, host and shell tests, reproducible packaging, and developer harness.
3. One-build baseline across Mini V1–V4, Plus, and Flip, with maintainer validation on V2/Plus/Flip and recorded external evidence for revisions not locally owned.
4. High-confidence correctness fixes and a capability-based platform abstraction.
5. Structured launch and session lifecycle, canonical game identity, activity correctness, and save safety.
6. Regression hardening, physical test matrix, migration safety, recovery, and release tooling.

BloomOS 1.0 is not blocked on Sync, Hub, Link, profiles, or a replacement frontend.

## Later 1.x releases

Planned work includes safe updates and rollback, optional synchronization and offline achievements, a package hub, profiles and Kids Mode, system health, performance profiles, Nearby Play, a local browser companion, and a unified in-game experience.

### RetroAchievements P0 platform

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
  The final package reproduces exactly across fresh builds and physically runs
  on Mini Plus. The optional runtime remains outside stable releases until its
  binary-input provenance is admitted or replaced by a source build.
- [ ] RA-13: finish account/settings UI and authentication lifecycle. The
  canonical service, redacted status, hidden host login bootstrap, one-time
  password-to-token exchange, and device-local mode-`0600` JFFS2 token storage
  are implemented and physically exercised with a disposable token; real RA
  authentication and sign-out/settings UI remain pending.
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
  config immutability; physical session validation remains pending.
- [ ] RA-21: add offline-cache UX. The adapter now safely supports resumable,
  foreground per-ROM and per-system caching with ROM-root confinement. UI
  progress/cancel presentation and Favorites/Recent/all selectors remain.
- [ ] RA-24: add aggregate RA health and support-export diagnostics. Health now
  allowlists catalog/proxy counts and rejects or discards secrets, ROM paths,
  titles, award details, and unexpected fields. Bounded structured RA log export
  remains pending.
- [ ] RA-25: add guarded RA certification tooling. Developer-mode preflight now
  verifies exact ROM identity and installed core SHA/policy without exposing
  ROM data; physical login, Rich Presence, Hardcore, lifecycle, save-flush, and
  explicit operator unlock execution remain pending.
- [x] RA-26: enforce a dedicated CI regression gate for exact core/source
  policy, required RA fixtures, service-only proxy integration, immutable
  transport, Hardcore routing, redaction-sensitive shell behavior, and focused
  offline tests without production RA network access.
- [ ] Keep RA-27 as a future consumer of the same `bloom-ra` API when Bloom owns
  the primary library browser; closed MainUI replacement is not a prerequisite
  for the P0 GameSwitcher badge.

- [ ] Restore Nintendo DS support with a reproducible, open-source libretro
  core such as melonDS or DeSmuME. Build it from pinned source for Bloom's
  ARMv7 runtime, keep proprietary BIOS/firmware out of releases, and qualify
  startup, rendering, audio, input, screen layouts, touch-pointer controls,
  saves, and representative-game performance before enabling `NDS` on Mini
  V1-V4, Plus, or Flip. DraStic and other proprietary standalone packages are
  not candidates for BloomOS distribution.

## BloomOS 2.0 research

An open MainUI replacement may be prototyped only after stable backend APIs and BloomOS 1.x compatibility are established.
