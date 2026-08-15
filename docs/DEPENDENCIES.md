# Dependency Inventory

Baseline: Onion `07505ea58c7bba698d6b9220ff43946a43cac76b`, audited 2026-08-15.

## Direct build inputs

| Input | Baseline reference | Pin status | Required action |
|---|---|---|---|
| Miyoo toolchain container | OCI index `sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132` (Linux/amd64 manifest `sha256:a8da1021449c80c0ccb75e263f1dfc75b5a004278fefa8a54151e55698a352f4`) | Pinned | Record compiler/binutils metadata when the image is first run. |
| SearchFilter submodule | `fc95ef8a3e67b54046fd03228df5b922f7bde834` | Commit-pinned | Record license and recursive dependencies in `build/dependencies.lock`. |
| Terminal submodule | `b8d6f98ed0d4f95542dd0acb7ec683482d0a4029` | Commit-pinned | Record license and build provenance. |
| DinguxCommander submodule | `7314f86cc1b5d1c75607120e9c2760261f69b67b` | Commit-pinned | Record license and build provenance. |
| RetroArch-patch submodule | `f9e959f7445d2ba0a4dd6279da41a095163767f2` | Commit-pinned | Relate this patch commit to the bundled RetroArch/core revisions and licenses. |
| RetroArch package marker | `RA_SUBVERSION=1.22.2-1` | Label only | Map to exact source commits, patches, binary hashes, and state-compatibility notes. |
| Themes download | `OnionUI/Themes` moving `main` files | Mutable | Replace with a commit or release plus SHA-256 manifest. |

## GitHub Actions

The inherited workflows referenced movable tags. Bloom resolves retained Actions to full commit SHAs with version comments. The formatting workflow no longer uses third-party changed-file or auto-commit Actions and never writes to contributor branches. Ordinary jobs declare `contents: read`; only release publication jobs receive job-scoped `contents: write`.

Build, pre-release, and tagged-release jobs now use the pinned toolchain index digest. The release scripts still resolve a moving latest release and theme content from moving `main`, so the complete baseline is not yet reproducible.

## Bundled binaries

The inherited history and tree contain large prebuilt emulator/core binaries, including ScummVM artifacts above GitHub's 50 MB recommendation. GitHub accepted the history, but Bloom must inventory each current bundled binary's source, exact revision, license, SHA-256, build method, and redistribution basis. Do not rewrite inherited history merely to silence the warning.

## Lock-file requirements

`build/dependencies.lock` now records the resolved toolchain, submodule, and Action identifiers plus explicit unresolved fields. It must expand to cover RetroArch and core source revisions, theme/package revisions, downloaded artifact hashes, license identifiers, and provenance URLs before release builds are considered locked.
