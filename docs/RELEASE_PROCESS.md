# Release Process

BloomOS has no public release process in operation yet. Release builds now produce a version-scoped bundle containing:

- `BloomOS-v<version>.zip` with Onion-compatible internal paths;
- `SHA256SUMS`;
- `manifest.json` with signed channel, artifact size, and digest;
- `manifest.sig`, an Ed25519 signature over the exact manifest bytes;
- `payload-manifest.json` with the type, size, and SHA-256 of every archive
  member;
- `build-info.json` with commit, deterministic build date, channel, toolchain,
  dependency-lock and payload-manifest digests, and compatible device targets.
- `payload-manifest.json` with the exact type, size, and SHA-256 digest of every
  archived file or symlink.

Packaging normalizes distribution-tree timestamps to the source commit time,
sorts the archive input list, suppresses archive timestamp metadata, and
validates the archive and metadata before publication.

Release packaging also validates `build/core-manifest.json` against every
shipped libretro binary. A core change therefore cannot silently retain stale
binary identity, declared license, or compatibility inventory.

Before archive assembly, packaging evaluates `build/provenance-policy.json`
for the requested channel. Stable, beta, and nightly accept only source-tier
components with pinned source, license, and repository build recipes. Legacy
inherited components are quarantined to development artifacts, and excluded
components cannot ship at all. This gate is intentionally stricter than a
checksum inventory: it prevents exact but historically unexplained binaries
from entering a public channel.

Release validation reconstructs the payload manifest directly from the final
ZIP, rejects duplicate, absolute, traversal, or backslash paths, and requires
the manifest digest recorded by `build-info.json` to match. This provides exact
binary provenance for the shipped bytes even while source/license mapping for
some inherited binaries remains an explicit audit item.

Release jobs materialize the protected `BLOOM_RELEASE_SIGNING_KEY` secret only
inside a mode-`0600` temporary file after the pinned cross-toolchain container
has produced the unsigned archive and metadata. Signing runs on the current
Ubuntu runner's OpenSSL 3 runtime, outside the older toolchain image. The build
proves that the private key's derived public
key matches the key shipped on devices, signs `manifest.json`, verifies that
signature before publication, and removes the temporary private key even when
the job fails. Devices verify the signature before parsing the manifest or
trusting its archive digest and size.

The signed manifest targets the `mini`, `plus`, and `flip` device families;
the detailed build record explicitly lists original Mini V1 through V4. Its
channel is one of `stable`, `beta`, `nightly`, or
`development`. Stable, beta, and nightly are user-selectable publication
channels; development is reserved for hardware-test artifacts. On-device
verification rejects any other value before an update can be staged.

Offline updates use the same trust boundary as future network delivery. Copy
the release ZIP, `manifest.json`, and `manifest.sig` to the SD card, then run
`bloomctl update stage MANIFEST SIGNATURE ARCHIVE`. Then run
`bloomctl update prepare VERSION` to validate the archive inventory and create
an isolated candidate. After reviewing `bloomctl update status`,
`bloomctl update arm VERSION` records the prepared, re-verified payload as
pending. Activation is an internal installer operation rather than a public
CLI shortcut. On the first installed boot, BloomOS records a durable validation
attempt. After checking the device, `bloomctl update confirm` verifies that the
installed and pending versions match, runs structured health checks, and only
then promotes the release to known-good. These commands do not extract directly
over the running OS.

If the attempt limit enters `recovery_required`, `bloomctl update rollback`
rebuilds the last known-good candidate from its retained signed release triplet,
creates a pre-rollback save snapshot, and arms the installer. It refuses to
overwrite pending installer content. The recovered release must boot and pass
the normal `bloomctl update confirm` gate; repeated rollback failures stop in a
terminal state for manual recovery.

## Remaining publication gates

Versioning, stable/beta/nightly channel policy, signed GitHub Actions builds,
and GitHub Releases provide the complete free publication path. Automated
migration, backup, rollback, recovery, host, sanitizer, shell, cross-build, and
packaging gates are in place.

Before the first public stable release, Bloom must still:

- replace or rebuild every included legacy-tier component so the provenance
  policy passes for `stable`;
- close every remaining immutable build-input and redistribution item in
  `build/dependencies.lock`;
- pass and record the three-device physical test matrix, including real
  migration, update, boot-confirmation, and rollback operations.

Releases must be built from a reviewed commit, and hardware-sensitive changes must identify the devices actually tested.
