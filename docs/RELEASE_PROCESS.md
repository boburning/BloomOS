# Release Process

BloomOS has no release process in operation yet. Before the first release, this document must define:

- versioning and stable, beta, and nightly channels;
- pinned inputs and reproducible packaging;
- checksums, manifests, and build metadata;
- automated test gates and the three-device physical test matrix;
- migration, backup, rollback, and recovery validation;
- license and bundled dependency review;
- signing and publication through infrastructure with a complete free path.

Releases must be built from a reviewed commit, and hardware-sensitive changes must identify the devices actually tested.
