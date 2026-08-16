# Release Process

BloomOS has no public release process in operation yet. Release builds now produce a version-scoped bundle containing:

- `BloomOS-v<version>.zip` with Onion-compatible internal paths;
- `SHA256SUMS`;
- `manifest.json` with artifact size and digest;
- `build-info.json` with commit, deterministic build date, channel, toolchain, dependency-lock digest, and compatible device targets.

Packaging normalizes distribution-tree timestamps to the source commit time, sorts the archive input list, suppresses archive timestamp metadata, and validates the archive and metadata before publication. Before the first public release, this document must still define:

- versioning and stable, beta, and nightly channels;
- closure of the remaining unresolved inputs in `build/dependencies.lock`;
- automated test gates and the three-device physical test matrix;
- migration, backup, rollback, and recovery validation;
- license and bundled dependency review;
- signing and publication through infrastructure with a complete free path.

Releases must be built from a reviewed commit, and hardware-sensitive changes must identify the devices actually tested.
