# Development

## Repository baseline

The project begins from Onion commit `07505ea58c7bba698d6b9220ff43946a43cac76b`, tagged `onion-upstream-4.4.0-beta-20260120`.

Use `origin` for the BloomOS repository and `upstream` for `https://github.com/OnionUI/Onion.git`. On case-insensitive filesystems, the upstream remote is configured not to fetch tags automatically because Onion contains historical tags that differ only by case.

## Workflow

Develop and cross-compile on the host; handhelds are deployment and test targets. Keep changes cohesive, run relevant host tests, and record physical-device results for hardware-sensitive work. Never test destructive migration logic against the only copy of real user data.

Build environment, dependency pinning, deployment tools, and smoke-test commands will be documented as they are established in Phase 1.

Maintainers can dispatch `Hardware test build` to produce a checksummed,
seven-day GitHub Actions artifact without publishing a release. This is the
only supported source for physical-device test images until the release process
is certified.

## Shell tests

Run `make test-shell` with Docker available. The BATS harness mounts the repository read-only and creates a disposable fake `/mnt/SDCARD` tree for each test. Never point shell integration tests at a real SD card or user save directory.

Native unit tests run normally and under AddressSanitizer in pull requests. To
reproduce the sanitizer pass on Linux:

```sh
GTEST_INCLUDE_DIR=/usr/src/gtest/include SANITIZE=1 make test
```

## Developer mode bootstrap

Developer mode is requested by creating `/mnt/SDCARD/.bloom-dev`. The initial implementation only reports the flag through `bloomctl info --json`; it does not yet enable SSH or change production behavior.

## Wi-Fi device harness

Copy `tools/targets.example.toml` to the ignored `tools/targets.toml` and replace
the documentation-only addresses and key paths. Hosts must already be present in
the user's SSH `known_hosts`; the harness deliberately refuses password prompts
and unknown host keys.

```sh
tools/bloom-device info bloom-plus
tools/run-smoke-tests.sh bloom-plus
tools/collect-logs.sh bloom-plus
```

The first smoke check is intentionally read-only. Diagnostic collection writes
`device.json` and `runtime.log` under the ignored `artifacts/device-logs/`
directory unless an explicit output path is supplied. Device activation and SSH
enablement are not part of this change.

## Original Mini SD-card tests

All original Miyoo Mini revisions use the same guarded SD-card protocol. While
the card is mounted on the host, prepare a request with the known revision:

```sh
tools/bloom-mini-test prepare /path/to/sd-root v2
```

The accepted revisions are `v1`, `v2`, `v3`, and `v4`. The command refuses a
directory without an Onion-compatible `.tmp_update` tree, enables the explicit
`.bloom-dev` flag, and creates only safe inventory tests under `BloomTest/`.
`bloom-test-runner` is idempotent and refuses any request that is not marked
`safe_only`. It does not infer the hardware revision; the result deliberately
requires manual confirmation until a reliable signal is established.

After the runner has executed on the handheld and the card is back in the host:

```sh
tools/bloom-mini-test consume /path/to/sd-root
```

This copies the request and results into the ignored `artifacts/mini-tests/`
directory. Boot integration is intentionally deferred until the runner has been
manually validated on the maintainer's V2.

`bloomctl info` is deliberately conservative. It reports observable capabilities and leaves the original Mini hardware revision unknown until a reliable V1–V4 detection method is established. Tests may set `BLOOM_ROOT` to a disposable fake root; production callers must leave it unset.
