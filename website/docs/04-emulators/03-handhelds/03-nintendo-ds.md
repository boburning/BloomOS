---
slug: /emulators/nds
---

# Nintendo DS

Nintendo DS emulation is not currently included in BloomOS.

The inherited DraStic package combined a proprietary emulator with private
libraries, firmware, databases, fonts, and interface assets whose complete
source, license, and build mapping could not be established. BloomOS excludes
that payload instead of publishing unverifiable binaries.

The `NDS` ROM folder is reserved so an existing library does not need to be
reorganized when support returns. A future Nintendo DS implementation must be
built reproducibly from pinned, redistributable source with all licenses and
runtime dependencies recorded. Candidate engines must also meet the memory and
performance limits of the original Miyoo Mini models, Miyoo Mini Plus, and
Miyoo Flip before support is restored.

Existing ROMs and saves are not deleted by this packaging change. They simply
remain unavailable from BloomOS until a verified emulator is added.
