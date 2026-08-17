# Onion Upstream Audit

Snapshot date: 2026-08-15. Source: the current open pull requests and issues in `OnionUI/Onion`. Recheck status and head commits immediately before porting.

## Open pull requests

| PR | Title | Area | Still relevant? | Bloom decision | Bloom issue/PR |
|---|---|---|---|---|---|
| [#1927](https://github.com/OnionUI/Onion/pull/1927) | Add RetroShelf to community apps | Documentation/telemetry-adjacent | No for core | Do not bundle. Reconsider only as an optional third-party integration after privacy review. | Backlog |
| [#1924](https://github.com/OnionUI/Onion/pull/1924) | Hebrew language | Localization | Yes | Re-evaluate after Bloom's font and right-to-left capability is tested; do not accept data files without font/render validation. | Backlog |
| [#1923](https://github.com/OnionUI/Onion/pull/1923) | Screen Time | Profiles/parental controls | Later | Use as research for the profile/Kids Mode milestone; do not port the large special-purpose implementation into 1.0. | 1.4 backlog |
| [#1921](https://github.com/OnionUI/Onion/pull/1921) | Fix Pico-8 activity covers | Play Activity | Yes | Reimplement or port with a regression test during the 1.0 correctness backlog. | Phase 3 |
| [#1920](https://github.com/OnionUI/Onion/pull/1920) | Pico-8 standalone documentation | Documentation | No | Native PICO-8 is excluded until its wrapper can be reconstructed from independently licensed inputs; Fake-08 remains supported. | Provenance backlog |
| [#1919](https://github.com/OnionUI/Onion/pull/1919) | Additional Perfect overlays | Assets | Optional | Do not mix into architecture work. Audit asset origins, licenses, size, and device-resolution behavior first. | Package backlog |
| [#1918](https://github.com/OnionUI/Onion/pull/1918) | Chunk-based OTA | Updates | Later | Study the provider-neutral chunk protocol for 1.1; do not import the bundled untracked binary or current workflow unchanged. | Phase 7 |
| [#1914](https://github.com/OnionUI/Onion/pull/1914) | Hungarian translation fixes | Localization | Yes | Safe candidate after validating against Bloom's current translation baseline. | Backlog |
| [#1912](https://github.com/OnionUI/Onion/pull/1912) | README image alt text | Onion docs | No | Bloom README no longer contains the affected image links. | N/A |
| [#1911](https://github.com/OnionUI/Onion/pull/1911) | Feature-doc typos | Onion docs | No | Affects versioned Onion documentation not currently carried into Bloom user docs. | N/A |
| [#1910](https://github.com/OnionUI/Onion/pull/1910) | Kids Mode app | Profiles/security | Later | Use as prior art, not a direct port. Reject the bundled gpSP binary and special-case config swapping; design first-class profiles with honest PIN semantics. | Phase 10 |
| [#1908](https://github.com/OnionUI/Onion/pull/1908) | Website naming and typo fixes | Documentation | Partly | Cherry-pick only still-relevant factual fixes into Bloom docs; do not restore Onion branding. | Docs backlog |
| [#1903](https://github.com/OnionUI/Onion/pull/1903) | Language list fixes | Localization | Yes | Port after checking Bloom's inherited language files and testing package display. | Backlog |
| [#1899](https://github.com/OnionUI/Onion/pull/1899) | Shell integration testing | Tests/CI | Yes, high value | Port the test concepts and fixtures. Re-pin the container and BATS dependencies, and validate BusyBox behavior rather than importing mutable inputs. | Phase 1 |
| [#1898](https://github.com/OnionUI/Onion/pull/1898) | FAQ restructure | Documentation | Partly | Use organization as reference; Bloom documentation will diverge and should not take the large move wholesale. | Docs backlog |
| [#1897](https://github.com/OnionUI/Onion/pull/1897) | SQLite WAL and busy timeout | Reliability/database | Yes, high value | Reimplemented with checked WAL configuration, full synchronous durability, bounded contention, clean-close sidecar, and abrupt-exit recovery tests. Physical SD/database validation remains required before merge. | [Bloom #22](https://github.com/boburning/BloomOS/pull/22) |
| [#1891](https://github.com/OnionUI/Onion/pull/1891) | Easy Netplay refactor | Networking | Later | Reassess for Nearby Play. It is a large untested-on-Flip shell rewrite and not a 1.0 dependency. | Phase 11 |
| [#1890](https://github.com/OnionUI/Onion/pull/1890) | Makefile hardening | Build | Yes, high value | Port in small commits after the toolchain is pinned; validate Windows/WSL/Linux behavior and preserve parallel make propagation. | Phase 1 |
| [#1883](https://github.com/OnionUI/Onion/pull/1883) | Merge v4.5-dev | Mixed hardware/runtime | Partly | Never merge wholesale. Audit individual commits, especially MY283 revision/model, display, and battery work, against Bloom's platform API and V1–V4 scope. | Phases 2–4 |
| [#1879](https://github.com/OnionUI/Onion/pull/1879) | Persistent DB connections | Database/runtime | Relevant but too broad | Extract root problems and tests. Prefer a smaller owned database layer after WAL/concurrency evidence; do not import the 1,800-line mixed refactor. | Phases 3 and 6 |
| [#1868](https://github.com/OnionUI/Onion/pull/1868) | Extract compressed ROMs | Launch/session | Later | Do not port the temporary extraction path before structured launch, storage bounds, cleanup, archive traversal, and save identity are defined. | Phase 5+ backlog |
| [#1515](https://github.com/OnionUI/Onion/pull/1515) | MainUI clock overlay | MainUI hook | No for 1.x foundation | Stale/dirty and dependent on closed MainUI hooking. Revisit only after platform time and frontend direction are stable. | 2.0 research |
| [#1460](https://github.com/OnionUI/Onion/pull/1460) | MainUI emulator sorter | MainUI utility | No | Stale/dirty with requested changes; canonical library ordering belongs in the future open frontend. | 2.0 backlog |
| [#1430](https://github.com/OnionUI/Onion/pull/1430) | Activity Tracker rewrite/delete | Activity/UI | Partly | Reuse requirements and tests, not the mixed implementation. Deletion, identity, and migrations should follow canonical GameID and the owned activity schema. | Phase 6 |

## Key conclusions

- No open PR should be merged wholesale into the baseline.
- Phase 1 should study #1899 and #1890 first.
- Phase 3 should independently validate #1921 and #1897.
- #1879 overlaps the same database area but is too broad to establish the initial contract.
- #1910 and #1923 are product prior art for a later profile model, not 1.0 patches.
- #1918 is relevant to 1.1 but needs binary provenance, license, protocol, rollback, and free-hosting review.

The issue classification is maintained separately in [ISSUE_TRIAGE.md](ISSUE_TRIAGE.md).
