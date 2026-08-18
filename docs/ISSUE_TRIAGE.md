# Onion Issue Triage

Snapshot date: 2026-08-15. The current Onion tracker contained 112 open issues. This is a planning classification, not a claim that each report reproduces on Bloom hardware.

## Bloom blockers

These reports describe a 4.4 regression, save-corruption risk, shutdown failure, or severe launch/session failure and must be reproduced or disproved before 1.0:

| Issues | Rationale |
|---|---|
| [#1930](https://github.com/OnionUI/Onion/issues/1930), [#1906](https://github.com/OnionUI/Onion/issues/1906), [#1905](https://github.com/OnionUI/Onion/issues/1905) | Explicit latest-beta display or crash regressions. |
| [#1926](https://github.com/OnionUI/Onion/issues/1926) | Inconsistent shutdown behavior. |
| [#1152](https://github.com/OnionUI/Onion/issues/1152) | Save-state regression across an Onion upgrade despite unchanged cores. |

DraStic issues #1916, #1406, and #1367 remain useful historical compatibility
evidence, but are no longer Bloom 1.0 blockers because BloomOS excludes the
inherited proprietary package. They become relevant again only after a
reproducible, licensed Nintendo DS implementation is selected.

## Bloom 1.0

- GameSwitcher/activity/identity: #1896, #1876, #1788, #1782, #1760, #1742, #1703, #1404, #1229, #1221, #1191, #848, #813.
- Platform/display/power/time: #1928, #1779, #1765, #1681, #1458, #1395, #672.
- Wi-Fi/runtime lifecycle: #1922, #1254, #1222, #1212, #1117, #845.
- Emulator integration requiring Bloom-side reproduction: #1831, #1826, #1801, #1761, #1522, #1516, #1297, #1247, #1218, #1162, #957.

## Bloom 1.x

Feature work already represented in the roadmap or suitable after 1.0 includes #1923, #1705, #1584, #1534, #1505, #1503, #1500, #1499, #1329, #1328, #1327, #1326, #1260, #1219, #1172, #1168, #1167, #1164, #1104, #961, #814, #792, #785, #781, #772, #770, #769, #768, #766, #764, #600, #571, #184, #161, #128, #107, #85, and #75.

## Upstream component bugs

These appear primarily owned by an emulator/core or external component. Bloom must pin, reproduce, and report or patch upstream rather than hide them: #1861, #1793, #1575, #1519, #1510, #1454, #1368, #1302, #1274, #1247, #1220, #1161, #1154, #1104, #786, and #785.

## Closed-source MainUI limitations

#1502, #1499, #78, #76, and portions of #773 depend on or replace Miyoo MainUI behavior. They are not BloomOS 1.0 blockers unless they prevent baseline compatibility; route them to the open-frontend roadmap.

## Cannot reproduce

#1458 and #957 already carry Onion's `unable to reproduce` label. Bloom will retain fixtures or test cases where possible and reclassify only after testing the applicable hardware/core.

## Documentation or likely obsolete

#1932, #1915, #1826, #849, and #661 need documentation review, not runtime changes. Older feature requests are not marked obsolete solely by age; status must be checked against the Bloom implementation before closure.

## Needs investigation

The following reports remain deliberately uncommitted pending body/comment review or reproduction: #1801, #1684, #1517, #1401, #1363, #1318, #1248, #1133, #1120, and #661.

## Triage policy

Each issue promoted into implementation needs a reproduction result, affected platform and firmware, ownership boundary, test, compatibility impact, and Bloom issue or PR link. A title-only classification must not be used as evidence that a bug is fixed.
