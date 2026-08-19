# Dependency Inventory

Baseline: Onion `07505ea58c7bba698d6b9220ff43946a43cac76b`, audited 2026-08-15.

## Direct build inputs

| Input | Baseline reference | Pin status | Required action |
|---|---|---|---|
| Miyoo toolchain container | OCI index `sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132` (Linux/amd64 manifest `sha256:a8da1021449c80c0ccb75e263f1dfc75b5a004278fefa8a54151e55698a352f4`) | Pinned | GCC/G++ 8.3.0, GNU ld/assembler 2.32.0.20190321, Python 3.7.3, p7zip 16.02, and Info-ZIP 3.0 are recorded in the lock. |
| Terminal submodule | `b8d6f98ed0d4f95542dd0acb7ec683482d0a4029` | Commit-pinned | Record license and build provenance. |
| RetroArch-patch submodule | `f9e959f7445d2ba0a4dd6279da41a095163767f2` | Commit-pinned | Patch revision is related to its recursive RetroArch source commit; core revisions and licenses remain to be inventoried. |
| RetroArch source submodule | `69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576` | Commit-pinned | Record the resulting binary hashes and core/state compatibility notes in release provenance. |
| RetroArch package marker | `RA_SUBVERSION=1.22.2-1` | Label mapped to source and patch commits | Complete the bundled core source, binary hash, and license mapping. |
| rcheevos hashing | Official v12.4.0 commit `2ad0b8672f68a48148620164510b963039e49eb1` (annotated tag object `7572a3b86e7dac4b2f053f4a21b7793af093e97b`) | Source and MIT license pinned | Bloom compiles the upstream `rc_hash` ROM, ZIP, and disc sources into `bloom-ra`. CHD remains unclaimed until a compatible reader callback is integrated and fixture-tested. |
| Themes download | `OnionUI/Themes` commit `b01198352e8927c3c5b9a828f73177bc81745954` | Commit and SHA-256 pinned; bounded retries use temporary files | Review and intentionally update `build/themes.sha256` when changing the curated theme set. |
| Dropbear SSH | Official Dropbear `2025.88`, commit `887241694277b3816da5d174932c10546ea9d2c5` | Tag and commit pinned | Bloom builds a static ARMv7 server/keygen multicall binary with password authentication and forwarding disabled at compile time. The upstream license is shipped beside the binary as `LICENSE.dropbear`. |
| GCC `libatomic.so.1` runtime | Miyoo Mini cross-toolchain image pinned by digest | Image and in-image path pinned | Copied from the ARM sysroot during release assembly because the bundled OpenSSL 3 executable requires it. Covered by the GCC Runtime Library Exception; exact binary hashing is recorded by release packaging. |
| Shell-test packages | `shell-test-inputs-v1`, archive SHA-256 `97a048903e55131877f44203103e33f28b84d6e0fe22c1588bb820b6075b6921` | Complete 38-APK closure archived with checksums and declared licenses | The image rebuild verifies the archive and every APK, removes Alpine repositories, and installs with Docker networking disabled. |
| Battery Monitor font | DejaVu Sans 2.37, archive SHA-256 `7576310b219e04159d35ff61dd4a4ec4cdba4f35c00e002a136f00e96a908b0a`, font SHA-256 `7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954` | Official release and license pinned | Replaces the inherited Arkhip file whose embedded notice reserves all rights. Visual layout remains pending device validation. |
| OpenBOR standalone | `DCurrent/openbor` commit `b00efbc7752cb55709dfc9fdfdfc7cfe78ddfb90`, SDL 2.0.20 commit `b424665e0899769b200231ba943353a5fee1b6b6`, libogg 1.3.5 commit `e1774cd77f471443541596e09078e78fdc342e4f`, and Tremor commit `820fb3237ea81af44c9cc468c8b4e20128e3e5ad` | Source, Miyoo patch provenance, licenses, toolchain, and output hash pinned | Bloom applies the reviewed Steward-Fu Miyoo patch to pinned OpenBOR source, statically links source-built codecs, and uses official SDL only as the link-time ABI. The device supplies Bloom's existing Miyoo SDL runtime. Physical validation remains pending. |
| PixelReader | `ealang/pixel-reader` v0.6 commit `762ed8ee40bf24fc05af1b0df1a95d30acd56b5b`, zlib 1.3.1 commit `51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf`, libxml2 2.12.10 commit `02d577715e1951cf114ca4e9adaf5605156c4f4c`, and libzip 1.11.3 commit `f30f5290de485348442d168cd7b2eb714d1f20f9` | Source, static private dependencies, licenses, toolchain, and output hash pinned | Replaces the inherited executable and six private shared libraries. zlib, libxml2, and libzip are linked statically; the app uses Bloom's shared SDL, SDL_ttf, and SDL_image runtime. Text and EPUB behavior remains pending physical validation. |

## GitHub Actions

The inherited workflows referenced movable tags. Bloom resolves retained Actions to full commit SHAs with version comments. The formatting workflow no longer uses third-party changed-file or auto-commit Actions and never writes to contributor branches. Ordinary jobs declare `contents: read`; only release publication jobs receive job-scoped `contents: write`.

Build, pre-release, and tagged-release jobs now use the pinned toolchain index
digest. Theme downloads are commit-pinned and checksum-verified. The inherited,
unfinished patch helper that resolved Onion's moving latest release has been
removed; Bloom release tooling now consumes only reviewed source and explicitly
pinned build inputs.

Bloom does not ship the inherited parallel Dropbear server or OpenSSH SFTP
helper. Their legacy password and passwordless toggles are removed; remote shell
access is provided only by the source-built Dropbear entry above, gated by
explicit developer mode and a provisioned public key.

Shell tests no longer resolve packages from the live Alpine repository. The
complete signed APK closure is retained as the `shell-test-inputs-v1` release
asset, with archive and per-package digests recorded in
`build/shell-test-inputs.json`. `tools/shell_test_inputs.py` rejects unsafe,
missing, duplicate, or modified archive members before the Docker build runs
with `--network=none`. The publisher produces the public GHCR image using only
repository-scoped credentials. Ordinary CI consumes that image exclusively by
immutable manifest digest; the mutable `v1` tag is never a CI trust input.

## Bundled binaries

The inherited history and tree contain large prebuilt emulator/core binaries, including ScummVM artifacts above GitHub's 50 MB recommendation. GitHub accepted the history, but Bloom must inventory each current bundled binary's source, exact revision, license, SHA-256, build method, and redistribution basis. Do not rewrite inherited history merely to silence the warning.

Every release now emits and validates `payload-manifest.json`, providing the
exact SHA-256 and size of each shipped file or symlink. This closes the binary
identity portion of provenance; upstream source, build method, license, and
redistribution mapping remain required for inherited binaries.

`build/core-manifest.json` additionally inventories all 110 shipped libretro
cores. It records each binary's exact size and SHA-256 plus any declared name,
version, and license available from the paired libretro metadata, and ties the
inherited bytes to Bloom's immutable Onion baseline commit. The release
target refuses a stale or non-canonical inventory. Unknown upstream revisions,
build flags, patch sets, known issues, and physical-validation dates remain
explicit `null` or empty fields; they must be filled from authoritative source
and device evidence rather than inferred from a filename.

## Release provenance policy

`build/provenance-policy.json` assigns every release component to a
machine-enforced provenance tier:

- `source` components have a pinned upstream revision, declared license, and
  repository build recipe and may ship on every channel;
- `legacy` components have exact inherited binary identity but incomplete
  authoritative source or license evidence and may ship only on the
  `development` hardware-test channel;
- `inventory` groups delegate channel eligibility to the exact resolution of
  every component selected from a canonical manifest;
- `excluded` components may not ship on any channel.

Release packaging checks this policy before assembling the archive. Stable,
beta, and nightly builds therefore fail closed while any included component is
still legacy. Inventory-backed runtime and package groups are evaluated at
their exact manifest resolution, so replacing one component removes only that
component from the blocker list; the policy cannot remain permanently blocked
by a resolved umbrella group. Moving a component to `source` requires replacing or rebuilding
it from reviewed inputs; binary hashes alone are never accepted as historical
source evidence.

`build/legacy-manifest.json` decomposes the remaining inherited runtime and
package payload into 147 independently resolvable components covering 2,128
files. Each entry locks its path, file count, checkout-normalized byte count,
and canonical tree SHA-256 while leaving unknown source, revision, license, and build recipe
explicitly `null`. Its resolution must eventually become either a complete
`source-build` record or an intentional channel exclusion. Release packaging
recomputes the inventory, so inherited payload drift cannot silently expand
the stable-publication backlog.

The component tree form treats CRLF and LF checkouts identically and represents
symlinks by their target bytes so Windows and Linux produce the same inventory.
This normalization is limited to the replacement queue; release payload
manifests continue to hash every final shipped byte exactly.

The source-wrapper and replacement pass identifies 137 package components and
shared package assembly scripts containing only
UTF-8 `.sh`/`.json` source, `.miyoocmd` command wrappers, `.notfound` port
definitions, and optional empty marker files. Those trees are
unchanged from the pinned Onion baseline and are recorded as GPL-3.0-only
source assemblies through the repository Makefile. This attribution applies
only to the wrapper files; it does not confer provenance on the emulator/core
binaries they select. Eight additional package or runtime components have
complete Bloom replacement records. The remaining two components are the
`.tmp_update` and Miyoo runtime trees, which still require component-specific
evidence, source replacement, or exclusion.

The FFplay package no longer ships its unreferenced inherited controls video.
That optional sample entered the Onion tree without license metadata and was
not used by the launcher, so Bloom excludes it instead of inferring public
redistribution rights. The remaining FFplay JSON and shell launcher are an
unchanged source-only wrapper attributed to the pinned Onion baseline.

The Ports Collection keeps its import and launch scripts, import command, and
plain-text bundled port definitions. Six inherited example artwork and manual
files without recorded license metadata are excluded; the package therefore
retains its functional importer without carrying opaque documentation assets.

Battery Monitor now builds from Bloom source and copies a checksum-pinned
DejaVu Sans 2.37 release plus its complete upstream font license. The inherited
Arkhip font was removed because its embedded copyright and usage records do not
grant the public redistribution rights Bloom requires. The package manifest
accepts this replacement only when every packaged resource matches its source
tree and the application opens the replacement font by its explicit filename.

Quick Guide now ships four Bloom-authored 640x480 pages generated by
`tools/generate_quick_guide.py` using only Python's standard library. Shell
tests regenerate every page byte for byte and verify the InfoPanel dimensions;
the inherited Onion-branded artwork is no longer part of the package.

The AdvanceMENU package now contains only its UTF-8 on-device launcher,
configuration, ROM scripts, and documentation. Nineteen inherited desktop-side
executables, archives, conversion scripts, and a media sample were removed from
the SD-card package; they were not used by the device launcher and did not have
complete component-level provenance. A future desktop companion must rebuild
equivalent conversion tools from reviewed source rather than copying them back.

OpenBOR standalone is rebuilt from pinned upstream commit `b00efbc` with the
small Miyoo-specific Makefile, input, menu, and renderer patch recovered from
Steward-Fu's checksum-locked source release. Official SDL 2.0.20 supplies only
the build-time ABI; the executable loads Bloom's existing Miyoo SDL runtime on
device. Pinned libogg and Tremor sources are compiled and linked statically,
their licenses ship beside OpenBOR, and two clean ARM builds produced the same
locked executable hash. Startup, controls, audio, rendering, and game behavior
still require physical validation.

The optional Fake-08 and PCSX-ReARMed standalone packages are excluded as
redundant. Bloom retains the existing Fake-08 and PCSX-ReARMed libretro cores,
launchers, ROM paths, and normal save integration instead of maintaining second
native engines and private runtime libraries. The retained cores remain in the
shared source-to-binary provenance backlog with the other inherited libretro
cores.

The optional standalone GnGeo package is excluded. Onion introduced its binary
from the MiyooCFW v2021-08-14 release, whose maintainers later documented that
the exact source was lost. Bloom retains Neo Geo support through the existing
libretro package instead of presenting a different GnGeo revision as the source
of the inherited executable.

The optional native PICO-8 wrapper is also excluded. It correctly required
users to supply their own purchased PICO-8 executable and data, but the wrapper
itself bundled shared libraries, a preload hook, and artwork from an upstream
repository with no declared license. Bloom retains PICO-8 support through the
existing Fake-08 libretro package; a future native wrapper
must be reconstructed entirely from reviewed, redistributable inputs.

The optional ScummVM standalone package is excluded as redundant. Bloom keeps
the existing ScummVM libretro launcher, importer, shortcut format, and core for
all supported device models. The inherited core remains subject to the same
source-to-binary provenance backlog as the other shipped libretro cores; this
decision avoids maintaining a second ScummVM executable and private codec
bundle without reducing supported game access.

The optional 240pSuite shortcut is excluded. Its inherited SNES ROM and
package-private Snes9x core do not have an exact source-to-binary mapping, and
the historical SNES build-kit fork carries unattributed host executables rather
than a reproducible source toolchain. Bloom retains ordinary SNES support. A
future calibration app must rebuild both its test payload and every required
host tool from reviewed source, or use independently authored Bloom patterns.

The SearchFilter repository and both package integrations are excluded because
upstream does not declare a project license. Source availability and commit
identity therefore do not establish public redistribution rights. Bloom keeps
its independently licensed per-system ROM cache refresh action, while global
search and keyword filtering require a clean-room or authoritatively licensed
replacement before they can return.

DinguxCommander and its package are excluded. The upstream repository contains
source but no project license, so neither that source nor the inherited package
assets can be redistributed on Bloom's public channels. A future file manager
must be independently implemented or selected from an authoritatively licensed
upstream project.

The inherited GMU package is excluded. Although GMU itself publishes GPL-2.0
source, the Miyoo package combines the executable, frontend and decoder
plugins, sixteen private codec/rendering libraries, themes, and two bundled
music tracks without a complete source, license, and build mapping for every
shipped byte. A future music player must be assembled entirely from pinned,
licensed sources and must not bundle media without explicit redistribution
rights.

The inherited runtime inventory is decomposed at its first meaningful payload
boundaries instead of treating `.tmp_update` and `miyoo` as two opaque trees.
Repository-owned configuration, language data, scaffolding, acknowledgments,
and shell entrypoints carry Bloom source provenance. The stale static
`libgamename.so` is removed because every release already replaces it with the
source-built output. Remaining runtime binaries, libraries, artwork, fonts,
media, public-key material, and mixed scraper data stay fail-closed as
independent unresolved components until each is rebuilt, replaced, or
excluded.

The `.tmp_update/bin` inventory is decomposed one level further so Bloom-owned
shell tools can carry repository provenance without attributing adjacent
third-party executables to Bloom. The checked-in Dropbear binary and license
copy are removed from the static payload; the pinned Dropbear build regenerates
both into the release tree before validation and packaging.

The `.tmp_update/script` inventory is likewise decomposed by command or
subsystem. UTF-8 shell and tabular source retains its exact Onion baseline
revision and GPL provenance. The scraper payload remains unresolved because it
mixes scripts with inherited binary databases and cannot be licensed as one
source unit.

The `.tmp_update/lib` inventory records each top-level library or runtime stack
independently. This does not infer source or license from filenames: all 40
units remain fail-closed until a pinned source build replaces them or their
redistribution provenance is documented.

Runtime resources are tracked per file. The two model settings, Wi-Fi reset
template, and Bloom release public key carry their exact source revisions.
Inherited icons and status artwork remain fail-closed until they are replaced
with reproducible Bloom assets or their original licenses are documented.

RetroAchievements dependencies are architecture-only until their focused
issues land. RA-02 will pin and source-build an official rcheevos release (the
RA-00 audit observed MIT-licensed `v12.4.0`) and record its resulting ARM output
identity. RAOfflineProxy is deferred until RA-17; its repository now declares
GPL-3.0 and contains Linux/Onion source, correcting the older no-license audit,
but Bloom must still pin and reproduce its application, Python runtime, native
libraries, and assets before distribution. Audit revisions and separation of
direct/proxy responsibilities are recorded in `docs/RETROACHIEVEMENTS.md`.

## Lock-file requirements

`build/dependencies.lock` records the resolved toolchain, submodule, Action,
inventory, and provenance-policy identifiers plus explicit unresolved fields.
It must expand to cover authoritative core source revisions, build recipes,
license identifiers, and immutable test-package inputs before stable release
builds are considered locked.
