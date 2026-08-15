# Dependency Inventory

Baseline: Onion `07505ea58c7bba698d6b9220ff43946a43cac76b`, audited 2026-08-15.

## Direct build inputs

| Input | Baseline reference | Pin status | Required action |
|---|---|---|---|
| Miyoo toolchain container | `aemiii91/miyoomini-toolchain:latest` | Mutable | Resolve an approved version and image digest; record compiler/binutils metadata. |
| SearchFilter submodule | `fc95ef8a3e67b54046fd03228df5b922f7bde834` | Commit-pinned | Record license and recursive dependencies in `build/dependencies.lock`. |
| Terminal submodule | `b8d6f98ed0d4f95542dd0acb7ec683482d0a4029` | Commit-pinned | Record license and build provenance. |
| DinguxCommander submodule | `7314f86cc1b5d1c75607120e9c2760261f69b67b` | Commit-pinned | Record license and build provenance. |
| RetroArch-patch submodule | `f9e959f7445d2ba0a4dd6279da41a095163767f2` | Commit-pinned | Relate this patch commit to the bundled RetroArch/core revisions and licenses. |
| RetroArch package marker | `RA_SUBVERSION=1.22.2-1` | Label only | Map to exact source commits, patches, binary hashes, and state-compatibility notes. |
| Themes download | `OnionUI/Themes` moving `main` files | Mutable | Replace with a commit or release plus SHA-256 manifest. |

## GitHub Actions

The inherited workflows use `actions/checkout@v3`, `tj-actions/changed-files@v37`, and `stefanzweifel/git-auto-commit-action@v4.1.2`. All are movable tags. Pin each action to a reviewed full commit SHA and document permissions before relying on CI.

Build, pre-release, and tagged-release jobs also use the mutable toolchain image. The release scripts resolve a moving latest release and theme content from moving `main`, so the baseline is not reproducible.

## Bundled binaries

The inherited history and tree contain large prebuilt emulator/core binaries, including ScummVM artifacts above GitHub's 50 MB recommendation. GitHub accepted the history, but Bloom must inventory each current bundled binary's source, exact revision, license, SHA-256, build method, and redistribution basis. Do not rewrite inherited history merely to silence the warning.

## Lock-file requirements

`build/dependencies.lock` should ultimately record the toolchain digest, submodule commits, RetroArch and core source revisions, theme/package revisions, downloaded artifact hashes, license identifiers, and provenance URLs. This document is the audit input; it is not itself a machine lock file.
