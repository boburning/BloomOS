# Release Process

BloomOS has no public release process in operation yet. Release builds now produce a version-scoped bundle containing:

- `BloomOS-v<version>.zip` with Onion-compatible internal paths;
- `SHA256SUMS`;
- `manifest.json` with signed channel, artifact size, and digest;
- `manifest.sig`, an Ed25519 signature over the exact manifest bytes;
- `build-info.json` with commit, deterministic build date, channel, toolchain, dependency-lock digest, and compatible device targets.

Packaging normalizes distribution-tree timestamps to the source commit time, sorts the archive input list, suppresses archive timestamp metadata, and validates the archive and metadata before publication. Before the first public release, this document must still define:

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

- versioning and stable, beta, and nightly channels;
- closure of the remaining unresolved inputs in `build/dependencies.lock`;
- automated test gates and the three-device physical test matrix;
- migration, backup, rollback, and recovery validation;
- license and bundled dependency review;
- signing and publication through infrastructure with a complete free path.

Releases must be built from a reviewed commit, and hardware-sensitive changes must identify the devices actually tested.
