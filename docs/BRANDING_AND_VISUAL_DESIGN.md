# BloomOS Branding and Visual Design

Status: normative direction; all shipped artwork must be original or have
documented redistribution rights.

## Identity

BloomOS uses an abstract radial bloom inspired by the layers of a blooming
onion. The mark should remain recognizable at small pixel sizes and may animate
from closed center to open petals during boot or loading. The joke is visual and
restrained; product copy does not depend on onion puns.

BloomOS is independent of OnionUI, Miyoo, and Outback Steakhouse. Do not copy an
Outback logo, wordmark, type treatment, mascot, exact composition, or proprietary
artwork. A warm restaurant-like palette is inspiration, not trade dress.

## Default tokens

| Token | Color | Use |
|---|---|---|
| `canvas` | `#211711` | Main background |
| `surface` | `#352319` | Cards and panels |
| `surface-raised` | `#493025` | Selected containers |
| `cream` | `#F3E2BD` | Primary text and light icons |
| `sand` | `#CDAF7B` | Secondary text |
| `orange` | `#D86A2C` | Primary accent and focus |
| `gold` | `#E2A93B` | Highlights and progress |
| `red` | `#A84832` | Destructive/error state |
| `green` | `#708A4A` | Confirmed/healthy state |

Every text/background and focus combination must pass the project's legibility
fixtures and physical-panel review. Focus cannot rely on orange alone: pair it
with a filled shape, border, or positional indicator.

## Visual grammar

Use crisp pixel-aligned geometry, limited radii, strong silhouettes, and one
consistent stroke weight. Icons are Bloom-owned, two-tone at most, readable at
16 and 24 pixels, and use shared metaphors for home, library, collections, apps,
settings, health, update, power, network, favorite, search, and recovery.

Typography prioritizes legibility over novelty. A bundled font needs pinned
source, license, and build provenance. Pixel display faces may be optional; the
default face must remain readable for long titles and localization.

## Branding audit rules

Stable normal paths contain no Onion name, onion bulb logo, Onion boot imagery,
MainUI branding, or inherited help copy presented as Bloom product identity.
Matches are classified as user-facing replacement, explicit compatibility or
migration copy, retained legal attribution, internal identifier, or removal.
The release gate fails on unclassified user-facing matches.
