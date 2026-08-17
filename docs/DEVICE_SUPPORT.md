# Device Support

BloomOS targets every original Miyoo Mini hardware revision (V1, V2, V3, and V4), the Miyoo Mini Plus, and the Miyoo Mini Flip. These are peers at the product level; revisions may have different capabilities and validation status.

No device is currently certified because BloomOS has not produced its first build. Stable support requires the physical release matrix, including installation, boot, representative game launch, saves and states, shutdown and recovery, native display behavior, and device-specific capabilities.

The maintainer-owned reference hardware currently includes a Mini V2, Plus, and Flip. V1, V3, and V4 support must still be implemented and tested; until equivalent physical evidence is contributed, releases must distinguish "supported" from "maintainer-validated" rather than implying that V2 testing covers all original Mini revisions.

The Miyoo Mini Plus completed a clean install and boot of `v0.1.0-dev-a93f9426` on 2026-08-16. Display, input, Wi-Fi association, and normal shutdown were confirmed on-device. Automated inventory identified model ID 354; it also exposed triple-buffered framebuffer, early-boot Wi-Fi, battery-probe, and FAT dirty-bit reporting/shutdown gaps that are covered by the next hardware-validation build.

Exact device revisions, firmware versions, build commits, test dates, contributor/tester identity or evidence link, and results will be recorded here. A successful boot alone does not qualify as support.

## Physical test evidence

| Date | Hardware | Build | Scope | Result | Follow-up |
| --- | --- | --- | --- | --- | --- |
| 2026-08-16 | Original Mini V2 (model ID 283) | `v0.1.0-dev-14f93ccc` | Fresh FAT32 install, first boot, identity, framebuffer inventory, input inventory, boot log, shutdown | Visually clean install and boot; safe-only request completed; `/dev/input/event0`; virtual framebuffer `640x1440`; no SD I/O failures | Battery capacity was unavailable through power-supply sysfs, so the harness now also captures the existing batmon output. Game launch, saves/states, recovery, and the remaining V1/V3/V4 matrix are still required. |
| 2026-08-16/17 | Miyoo Mini Plus (model ID 354) | `v0.1.0-dev-e61e3fa7`; `v0.1.0-dev-3ed5ba7d` with `dec77ee2` SSH artifacts and staged game-smoke utility | Upgrade install, clean-entry boot, identity, display/input/battery inventory, Wi-Fi association, shutdown, Windows FAT check, key-only SSH, bounded runtime launch for GB/GBC/GBA/NES/SNES/PSX | Clean install and boot; 640×480 physical panel reported separately from 640×1440 framebuffer; Wi-Fi capability true; live AXP battery 74%. Bloom-built static Dropbear 2025.88 accepted repeated Ed25519 logins with the card-pinned host key, refused password-only authentication, replaced tracked listener PID 1049 with PID 2025 during a verified restart, reported model 354, and retained about 74 MB available memory. Six representative homebrew/runtime samples launched through the normal command handoff for eight seconds each; RetroArch stayed alive, exited on SIGTERM, removed the command, and returned to MainUI every time. Peak observed RSS ranged from 10,392 KB (GB) to 38,216 KB (SNES). The SSH validation shutdown returned to Windows Healthy/OK and NOT Dirty without repair, with both staged SSH hashes matching the repository. | Clean shutdown disables SD-backed swap, syncs, and remounts FAT read-only before vendor poweroff. Visual/audio correctness, controls, SRAM and save states, recovery, and longer soak testing are still required. |
| 2026-08-16 | Miyoo Mini Flip (model ID 285) | `v0.1.0-dev-bc086aa6`, `v0.1.0-dev-95de9362`, `v0.1.0-dev-fbf1a03e`, `v0.1.0-dev-3ed5ba7d` | Upgrade install, identity, display/input/battery/lid inventory, Wi-Fi capability, clean-entry boot, instrumented shutdown, Windows FAT check, Play Activity WAL compatibility, key-only SSH | Clean install and boot; native 752×560 panel; two input devices; hall sensor present with open-lid raw value 1; live AXP battery 64%; Wi-Fi capability true. A controlled shutdown cycle entered and returned clean. Play Activity converted a seeded rollback-journal database to WAL, preserved its rows, passed integrity and quick checks, and left no WAL/SHM sidecars after three UI opens. Bloom-built static Dropbear 2025.88 accepted repeated Ed25519 logins, refused password-only authentication, reported model 285 over the live harness, and retained about 74 MB available memory. | Earlier cycles entered or returned dirty, although CHKDSK found no filesystem damage. The first Activity Tracker open/exit briefly glitched the display, then two repeats were normal with no logged crash. Longer soak testing, reboot-path isolation, reboot-time SSH activation, and lid behavior remain required. |

The V2 result is a maintainer validation baseline, not certification of V2 or evidence for V1, V3, or V4. The first Plus shutdown retest was invalid because the card entered Linux dirty after a failed Windows command-line eject. After repairing and using Windows Safely Remove Hardware, the card entered Linux without the not-properly-unmounted warning and returned to Windows clean after BloomOS shutdown.

On 2026-08-17, a later Plus test invoked BloomOS's clean `shutdown -r` path
over SSH. The helper quiesced SD-card consumers and disconnected normally, but
the device did not return to its prior address within five minutes and a subnet
SSH scan found no replacement address. Physical inspection and power-on are
deferred until the maintainer returns; the persistent shutdown log must be
collected before the reboot path is changed.
