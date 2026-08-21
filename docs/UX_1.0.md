# BloomOS 1.0 UX Contract

Status: normative for the stable handheld UI.

## Information architecture

The top level is Home, Library, Collections, Apps, and Settings. `L1` and `R1`
move between destinations. Each screen has a title, visible focus, content, and
a consistent footer. Unsupported capabilities are omitted rather than shown as
disabled controls.

Home leads with the last resumable game, then recent games, favorites, and
shortcuts. Library opens cached data immediately and scans in the background.
Collections contains favorites, recent, and user collections. Apps launches
compatible tools through a supervised adapter. Settings owns every user-facing
setting and contains System, Health, Updates, and About.

## Control grammar

| Input | Meaning |
|---|---|
| D-pad | Move focus |
| A | Confirm, launch, or resume |
| B | Back |
| L1/R1 | Previous/next top-level destination |
| X | Context actions |
| Y | Toggle favorite on a game row |
| SELECT | Search when shown in the footer |
| START | Quick Settings or selected GameSwitcher actions |
| MENU | GameSwitcher during play |

Common actions must not depend only on long presses or undocumented chords.

## Performance behavior

The renderer never waits for network, hashing, scanning, scraping, or update
checks. Cached lists target a 150 ms open, loaded modals 50 ms, and visible focus
response p95 below 50 ms. Images decode off the render path and use a byte-bounded
cache. Shell resources that games do not need are released before launch.

## Copy and errors

Copy is concise, calm, and plainspoken. Normal screens do not expose internal
terms such as candidate, arm, schema, proxy transport, service names, or paths.
Errors contain a plain title, one-sentence explanation, safest next action, and
optional technical details. RetroAchievements failure is non-blocking status,
not a launch-stopping modal.

## Accessibility

Focus uses shape or fill as well as color. No essential status uses color alone.
Default type is readable at 640x480 and 752x560, truncation is deterministic,
motion is brief and optional, and there is no rapid flashing or permanent
marquee. Layouts allow localized strings and a larger-text option where it can
be implemented without clipping core actions.

## Acceptance tasks

A new user can resume the latest game with one confirmation from Home, open the
Library immediately, favorite a selected game with one button, find Search from
the visible footer, reach Quick Settings with `START`, inspect network status on
capable devices, reach Settings > System > Updates, export a support bundle from
Health, and enter Safe Mode without editing the SD card on a computer.

Physical review remains required for display legibility, palette, animation,
audio, controls, save semantics, and state restoration. SSH evidence must not be
reported as proof of those properties.
