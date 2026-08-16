# Device Support

BloomOS targets every original Miyoo Mini hardware revision (V1, V2, V3, and V4), the Miyoo Mini Plus, and the Miyoo Mini Flip. These are peers at the product level; revisions may have different capabilities and validation status.

No device is currently certified because BloomOS has not produced its first build. Stable support requires the physical release matrix, including installation, boot, representative game launch, saves and states, shutdown and recovery, native display behavior, and device-specific capabilities.

The maintainer-owned reference hardware currently includes a Mini V2, Plus, and Flip. V1, V3, and V4 support must still be implemented and tested; until equivalent physical evidence is contributed, releases must distinguish "supported" from "maintainer-validated" rather than implying that V2 testing covers all original Mini revisions.

Exact device revisions, firmware versions, build commits, test dates, contributor/tester identity or evidence link, and results will be recorded here. A successful boot alone does not qualify as support.

## Physical test evidence

| Date | Hardware | Build | Scope | Result | Follow-up |
| --- | --- | --- | --- | --- | --- |
| 2026-08-16 | Original Mini V2 (model ID 283) | `v0.1.0-dev-14f93ccc` | Fresh FAT32 install, first boot, identity, framebuffer inventory, input inventory, boot log, shutdown | Visually clean install and boot; safe-only request completed; `/dev/input/event0`; virtual framebuffer `640x1440`; no SD I/O failures | Battery capacity was unavailable through power-supply sysfs, so the harness now also captures the existing batmon output. Game launch, saves/states, recovery, and the remaining V1/V3/V4 matrix are still required. |

The V2 result is a maintainer validation baseline, not certification of V2 or evidence for V1, V3, or V4. The test SD was already marked not-properly-unmounted when Linux mounted it; Windows subsequently found no FAT file or folder errors in a read-only scan.
