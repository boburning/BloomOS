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

Developer mode is requested by creating `/mnt/SDCARD/.bloom-dev`. On the
Wi-Fi-capable Mini Plus and Mini Flip, key-only SSH also requires at least one
valid public key at:

```text
/mnt/SDCARD/.bloom/authorized_keys
```

Private keys stay on the development host. Bloom accepts plain `ssh-ed25519`,
`ssh-rsa`, and standard NIST ECDSA public-key lines without authorized-key
options. A missing or malformed file fails closed and stops developer Dropbear.
The original Mini V1–V4 ignore SSH provisioning and continue to use the SD-card
test protocol.

On Linux or WSL2, provision a dedicated key. The device harness is deliberately
non-interactive, so either load a passphrase-protected key into `ssh-agent`
before use or create a device-specific automation key with an empty passphrase:

```sh
ssh-keygen -t ed25519 -N '' -f ~/.ssh/bloom_flip -C bloom-flip
mkdir -p /path/to/sd/.tmp_update/config/bloom
mkdir -p /path/to/sd/.bloom
cp ~/.ssh/bloom_flip.pub /path/to/sd/.bloom/authorized_keys
```

Bloom's pinned, reproducible Dropbear build stores generated host keys under
`.tmp_update/etc/dropbear`, so the first connection must verify and accept the
unique device/SD-card fingerprint. Password authentication and forwarding are
disabled at compile time while root public-key login remains available. Remove
`.bloom-dev` and reboot to disable automatic developer SSH.

## Wi-Fi device harness

Copy `tools/targets.example.toml` to the ignored `tools/targets.toml` and replace
the documentation-only addresses and key paths. Hosts must already be pinned in
the user's SSH `known_hosts`, or in the target's optional `known_hosts_file`.
Set `host_key_alias` when one test SD card, and therefore one Bloom host key,
moves between handhelds or DHCP addresses. The harness deliberately refuses
password prompts and unknown host keys.

```sh
tools/bloom-device info bloom-plus
tools/run-smoke-tests.sh bloom-plus
tools/collect-logs.sh bloom-plus
tools/bloom-device game-smoke bloom-plus GB \
  "/mnt/SDCARD/Roms/GB/Example.zip" 10
```

The first smoke check is intentionally read-only. Diagnostic collection writes
`device.json` and `runtime.log` under the ignored `artifacts/device-logs/`
directory unless an explicit output path is supplied. Device activation
requires `.bloom-dev` and a valid developer public key; no address, password,
private key, or accepted host fingerprint is stored in the repository.

`game-smoke` is an explicitly developer-only runtime probe for GB, GBC, GBA,
NES/FC, SNES/SFC, and PSX. It confines the supplied ROM to the matching system
directory, launches through the normal Onion compatibility handoff, keeps SSH
alive for observation, bounds execution to 5–60 seconds, terminates RetroArch,
and verifies that MainUI returns and the pending command is removed. A pass
proves launch and cleanup liveness only; it does not prove correct video,
controls, audio, SRAM, or save states. Test ROMs and BIOS files remain local and
must never be committed.

| Device | Development method |
| --- | --- |
| Original Mini V1–V4 | Guarded SD-card request/results harness; UART is optional advanced work |
| Mini Plus | Wi-Fi plus key-only SSH |
| Mini Flip | Wi-Fi plus key-only SSH |

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
directory. At boot, the runtime invokes the idempotent runner only when both
`.bloom-dev` and `BloomTest/request.json` are present. The runner accepts only
requests explicitly marked `safe_only`.

`bloomctl info` is deliberately conservative. It reports observable capabilities and leaves the original Mini hardware revision unknown until a reliable V1–V4 detection method is established. Tests may set `BLOOM_ROOT` to a disposable fake root; production callers must leave it unset.
