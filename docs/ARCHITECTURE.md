# Architecture

## Direction

BloomOS retains Onion's existing layout initially and replaces fragile boundaries incrementally. Large repository moves are not a bootstrap goal.

The target architecture separates shared services from original Mini, Plus, and Flip platform backends. The original Mini backend must represent V1–V4 hardware and firmware differences explicitly; it must not assume that the maintainer-owned V2 is representative of every revision. Consumers should query explicit capabilities—such as display dimensions, Wi-Fi, RTC, lid, and rumble—instead of scattering model checks.

Planned stable service boundaries include platform discovery, structured game launch, session lifecycle, canonical game identity, activity storage, updates, sync, and health diagnostics. Miyoo's closed MainUI remains a compatibility boundary for BloomOS 1.x.

Architectural decisions will be recorded as the corresponding subsystem work begins.
