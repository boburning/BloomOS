# BloomOS 1.0 UX Contract

Status: normative for the stable handheld UI.

## Product model

BloomOS has one root launcher, not a set of tabs or a destination picker. The
root shows a contextual **Continue Playing** target when a resumable game is
available and a fixed rail containing **Games**, **Favorites**, **Recent**,
**Apps**, and **Settings**. Collections is not a required 1.0 root concept.

If Continue is available after cold boot it receives initial focus and `A`
resumes immediately; Down moves to the rail. Otherwise Games receives focus.
Left/Right chooses a rail destination, `A` opens it, and Up returns to Continue
when present. A child remembers its own selection and `B` returns to the prior
root selection. `B` at root is a safe no-op and never terminates Bloom Shell or
powers off.

The maximum normal hierarchy is:

```text
Root
└── Primary screen
    └── Optional detail or action sheet
```

A keyboard, confirmation, search/filter, or focused picker may temporarily
overlay its invoking screen. A detail screen must not primarily contain another
list of destinations whose purpose is to open more destination lists.

## Stable control grammar

| Input | Meaning |
|---|---|
| D-pad | Move focus or adjust a clearly indicated value |
| A | Confirm, open, launch, resume, or toggle |
| B | Back or close |
| X | Actions for the focused object |
| Y | Favorite on games; otherwise only a labeled secondary action |
| SELECT | Search or filter on searchable game surfaces |
| START | Quick Settings |
| MENU | GameSwitcher |
| L1/L2/R1/R2 | No Bloom-owned UI navigation |

Shoulder inputs remain available to emulators and compatibility software, but
Bloom Shell, GameSwitcher, Settings, dialogs, search, and Safe Mode assign them
no navigation responsibility and never show shoulder hints. Essential behavior
must not depend on long presses, chords, or undocumented timing.

Safe Mode uses one flat recovery list rather than a second launcher hierarchy.
It removes Continue and automatic resume, retains Games browsing, and exposes
only bounded recovery actions: health refresh, privacy-safe support export,
confirmed signed-version restore, confirmed Settings reset, and explicit normal
restart. Settings reset preserves an exact private pre-reset copy and does not
touch games, saves, states, play history, library data, or account credentials.
`B` at the Safe Mode root is safe, `START` still owns Quick Settings, `MENU`
still owns GameSwitcher, and shoulders remain unassigned.

`src/bloomUi/bloom_ui_core` owns semantic input and focus behavior. Device
adapters translate physical input without inventing hidden shortcuts. Lists
clamp at their ends, keep selection visible, and use bounded repeat with smooth
acceleration for deliberate Up/Down holds.

## Games, Favorites, and Recent

Games is one shallow two-axis browser: Left/Right selects the previous or next
non-empty system and Up/Down selects a game. Each system preserves its own
selection and scroll position; switching systems never resets another system to
row zero. `A` launches, `X` opens a flat action sheet, `Y` toggles favorite,
SELECT opens local search/filter, and `B` returns to root. Cached data opens
immediately; scanning, hashing, networking, and image decoding do not run per
keypress or on the render path.

Favorites and Recent are direct root destinations using the shared game-list
presentation. Empty states teach the next action. Recent is browsable history;
GameSwitcher is immediate session resume. Removing history is available only
through `X` -> Remove from Recent -> safe-default confirmation, never a direct
destructive button binding.

## GameSwitcher

MENU opens or predictably toggles GameSwitcher. Left/Right chooses a game, `A`
resumes, `X` opens valid actions, `Y` toggles favorite when shown, START opens
Quick Settings, and `B` goes Home. Version 1.0 ships one polished presentation.

Inherited one-off behavior is forbidden: no Up/Down brightness, SELECT
header/time cycling, Y short/long view modes, direct X deletion, GameSwitcher-
specific START menu, shoulder navigation, or hidden combos. Brightness belongs
in Quick Settings and history deletion belongs in Actions.

## Apps and contextual actions

Apps contains only reviewed, supervised product apps. Every shipped app has one
disposition: keep in Apps, migrate to Settings, migrate to a contextual action,
migrate to Developer, replace with native Bloom, or remove from stable. Tweaks
is migration/reference code, not a stable UI to expand; normal BloomOS use must
not require it. Settings owns configuration, deterministic ordering replaces
app sorting, and maintenance utilities live in context or Developer surfaces.

X consistently opens a flat, object-specific action sheet. A focused picker may
follow one action and returns directly to its invoking screen. Search is a
SELECT overlay on Games, Favorites, and Recent, uses local indexes, and is not a
separate app.

## Flat Settings

Settings is one vertically scrolling, sectioned surface. Normal sections are
Display, Audio, Controls & Gameplay, Network where supported,
RetroAchievements, Appearance, and System. Developer appears only when
Developer Mode is active; there is no normal Advanced junk drawer.

Rows have an explicit type: non-selectable section header, toggle, enum, slider,
detail, action, or read-only. Up/Down moves through selectable rows; Left/Right
changes sliders/enums; `A` toggles or opens one focused detail sheet; `B`
returns root. Simple values do not require a detail page. Capability predicates
omit unsupported rows rather than disabling them.

Detail rows may open Wi-Fi selection, RA account entry, Update, Storage, Health
and support export, About, or Developer diagnostics. `B` returns directly to
Settings. Details do not become category trees or recreate Tweaks.

## Quick Settings

START toggles Quick Settings from every normal Bloom surface. It contains
brightness, volume, mute, capability-gated Wi-Fi, read-only battery, and Open
Settings. Up/Down selects, Left/Right adjusts, `A` toggles or opens, and B or
START closes. Resets, update channels, full RA configuration, and developer
tools do not belong here.

## Presentation

The header shows the current screen or system plus concise battery and
capability-gated Wi-Fi state; it is not navigation. The footer shows only
currently useful actions and never shoulders. Focus uses fill or contrast
inversion in addition to the Bloom orange accent.

Bloom uses the existing radial mark and warm brown/cream/orange/gold tokens,
crisp pixel-aligned geometry, strong silhouettes, limited radii, and no blur,
card-heavy dashboard, animated wallpaper, bounce, or input-delaying motion.
Useful transitions target 100-150 ms and are dropped rather than missing frames.
Both 640x480 and native 752x560 layouts are first-class.

Empty and error copy is calm and instructional. Normal UI never exposes
`mainui-dependent`, update-state internals, schemas, proxy terms, or private
paths. RetroAchievements failure never blocks ordinary play.

## Performance contract

Visible focus response targets p95 below 50 ms. Cached root destinations open
within roughly 100-150 ms, Quick Settings and GameSwitcher feel immediate,
system switching does not visibly reload cached lists, and held scrolling does
not starve rendering. A 30-minute shell soak, 500 open/back/action cycles, and a
10k-game fixture remain bounded and responsive.

## Completion gate

The UX contract is complete only when one root exposes Continue plus the five
fixed destinations; MENU is only GameSwitcher; START is only Quick Settings;
shoulders have no Bloom navigation role; Games is the persistent two-axis
browser; Favorites and Recent are direct; Settings is flat with at most one
detail layer; Tweaks is not normally required or visible; GameSwitcher has no
inherited special controls; footers teach the stable grammar; and all core tasks
pass with D-pad plus A/B/X/Y/SELECT/START/MENU.

Physical review is required on Mini V2, Plus, and Flip for legibility, focus,
root recognition, Continue and two-axis discoverability, long-list behavior,
footer density, tabletop use, GameSwitcher semantics, Settings flatness, audio,
controls, saves, and state restoration. SSH and framebuffer evidence never
substitute for those physical claims.
