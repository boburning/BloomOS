# BloomOS

BloomOS is a maintained, community-led continuation of Onion for the Miyoo Mini family. It aims to preserve Onion's fast, appliance-like experience while improving maintainability, testing, recovery, and support for every original Miyoo Mini revision (V1–V4), the Miyoo Mini Plus, and the Miyoo Mini Flip.

> [!IMPORTANT]
> BloomOS is at the repository-bootstrap stage and is not yet a release for end users.

## Project principles

- Preserve Onion compatibility wherever technically reasonable.
- Treat the original Mini family, Plus, and Flip as first-class targets in one codebase.
- Protect saves and make migrations recoverable.
- Keep normal use offline-first and free of mandatory paid infrastructure.
- Collect no telemetry by default.
- Make changes incrementally, with automated and physical-device testing.

See [the roadmap](docs/ROADMAP.md), [device support policy](docs/DEVICE_SUPPORT.md), and [development guide](docs/DEVELOPMENT.md) for current project status.

## Upstream and attribution

BloomOS is based on [OnionUI/Onion](https://github.com/OnionUI/Onion), beginning at commit [`07505ea58c7bba698d6b9220ff43946a43cac76b`](https://github.com/OnionUI/Onion/commit/07505ea58c7bba698d6b9220ff43946a43cac76b) (`Onion v4.4.0-beta-20260120`). The immutable local baseline tag is `onion-upstream-4.4.0-beta-20260120`.

BloomOS is an independent community project. It is not an official Onion release and is not affiliated with or endorsed by the Onion team or Miyoo.

Existing copyright notices, third-party licenses, and attribution are retained. BloomOS is distributed under the GNU General Public License v3.0; see [LICENSE](LICENSE).

## Contributing

BloomOS is early in development. Before proposing a change, read [CONTRIBUTING.md](CONTRIBUTING.md), [the architecture notes](docs/ARCHITECTURE.md), and [the compatibility policy](docs/COMPATIBILITY.md).
