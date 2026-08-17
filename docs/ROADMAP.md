# Roadmap

## Implementation status

Last updated: 2026-08-17

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

### In progress

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
- [ ] Bound verified save snapshots while preserving active recovery references
  and corrupt evidence (`feature/save-snapshot-retention`).

### Deferred physical validation

- [ ] Inspect and power on the Miyoo Mini Plus after the 2026-08-17 remote
  reboot command completed shutdown but did not return to Wi-Fi; collect the
  persistent shutdown log before changing reboot behavior.
- [ ] Run real signed update activation, boot confirmation, bounded-failure,
  and rollback tests on Plus, then repeat applicable coverage on Mini V2 and
  Flip.
- [ ] Complete visual/audio/input/SRAM/save-state and longer soak coverage on
  maintainer-owned V2, Plus, and Flip hardware.
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
