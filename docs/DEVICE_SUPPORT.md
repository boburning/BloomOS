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
| 2026-08-16 | Miyoo Mini Plus (model ID 354) | `v0.1.0-dev-e61e3fa7` | Upgrade install, clean-entry boot, identity, display/input/battery inventory, Wi-Fi association, shutdown, Windows FAT check | Clean install and boot; 640×480 physical panel reported separately from 640×1440 framebuffer; Wi-Fi capability true; live AXP battery 74%; normal shutdown completed; Windows returned Healthy/OK and NOT Dirty without repair | Clean shutdown now disables SD-backed swap, syncs, and remounts FAT read-only before vendor poweroff. Game launch, saves/states, recovery, and longer soak testing are still required. |
| 2026-08-16 | Miyoo Mini Flip (model ID 285) | `v0.1.0-dev-bc086aa6`, `v0.1.0-dev-95de9362` | Upgrade install, identity, display/input/battery/lid inventory, Wi-Fi capability, clean-entry boot, instrumented shutdown, Windows FAT check | Clean install and boot; native 752×560 panel; two input devices; hall sensor present with open-lid raw value 1; live AXP battery 64%; Wi-Fi capability true. The final controlled cycle entered Linux clean; telemetry confirmed the shutdown helper ran, the first read-only remount succeeded, recursive unmount succeeded, and Windows returned Healthy and NOT Dirty. | Earlier cycles entered or returned dirty, although CHKDSK found no filesystem damage. The successful controlled cycle validates normal shutdown on this build but longer soak testing, reboot-path isolation, and lid behavior remain required. |

The V2 result is a maintainer validation baseline, not certification of V2 or evidence for V1, V3, or V4. The first Plus shutdown retest was invalid because the card entered Linux dirty after a failed Windows command-line eject. After repairing and using Windows Safely Remove Hardware, the card entered Linux without the not-properly-unmounted warning and returned to Windows clean after BloomOS shutdown.
