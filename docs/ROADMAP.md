# Roadmap

## Implementation status

Last updated: 2026-08-18

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

- [x] Reconcile activated update boots, count bounded validation attempts, and
  require installed-version plus structured-health confirmation before
  promotion ([PR #54](https://github.com/boburning/BloomOS/pull/54)).
- [x] Reconstruct rollback candidates from retained signed archives, snapshot
  saves, publish the recovery trigger transactionally, and expose the guarded
  `bloomctl update rollback` operation ([PR #55](https://github.com/boburning/BloomOS/pull/55)).
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
- [x] Replace the inherited Fake-08 standalone executable with a deterministic
  ARM build from pinned upstream source and ship its complete composite license
  ([PR #86](https://github.com/boburning/BloomOS/pull/86)).
- [x] Exclude the optional standalone GnGeo package whose exact released source
  was documented as lost, while retaining Neo Geo support through libretro
  ([PR #87](https://github.com/boburning/BloomOS/pull/87)).
- [x] Replace the inherited OpenBOR executable with a deterministic ARM build
  from pinned upstream source, the reviewed Steward-Fu Miyoo patch, and
  source-built codec dependencies; defer device behavior to the hardware matrix
  ([PR #88](https://github.com/boburning/BloomOS/pull/88)).
- [x] Exclude the optional native PICO-8 wrapper whose support payload has no
  declared upstream license, while retaining both source-built Fake-08 options
  and leaving a clean path for a future independently licensed native wrapper
  ([PR #89](https://github.com/boburning/BloomOS/pull/89)).
- [x] Exclude the redundant inherited ScummVM standalone package and retain the
  existing ScummVM libretro integration, avoiding a second engine and codec
  stack while core source-to-binary provenance remains in the shared backlog.
- [x] Replace PCSX-ReARMed standalone, its external GPU plugins, package-private
  Miyoo SDL, and menu skin with deterministic builds/assets from pinned source;
  remove the unreachable stock fallback, unattributed cheat database, and
  pre-created memory cards.

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
- [ ] Repeat the applicable reboot/poweroff paths on Mini V2 and Flip. Treat a
  USB-powered Plus entering its firmware charging screen as a separate charging
  mode; it requires a second power press and is not evidence of a failed clean
  shutdown.
- [ ] Run real signed update activation, boot confirmation, bounded-failure,
  and rollback tests on Plus, then repeat applicable coverage on Mini V2 and
  Flip.
- [ ] Complete visual/audio/input/SRAM/save-state and longer soak coverage on
  maintainer-owned V2, Plus, and Flip hardware.
- [ ] Validate Battery Monitor text fit and readability on V2, Plus, and Flip
  after replacing Arkhip with DejaVu Sans 2.37.
- [ ] Validate Quick Guide page readability and shortcut accuracy on V2, Plus,
  and Flip after replacing the inherited artwork.
- [ ] Validate source-built Fake-08 startup, audio, input, save behavior, and
  representative cartridge compatibility on V2, Plus, and Flip.
- [ ] Validate source-built OpenBOR startup, rendering, audio, input mappings,
  PAK loading, and save behavior on V2, Plus, and Flip.
- [ ] Validate source-built PCSX-ReARMed standalone startup, rendering, audio,
  input, memory-card creation, save states, and representative games on V2,
  Plus, and Flip.
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

## BloomOS 2.0 research

An open MainUI replacement may be prototyped only after stable backend APIs and BloomOS 1.x compatibility are established.
