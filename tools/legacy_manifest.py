#!/usr/bin/env python3
"""Create and validate the exact inherited-component replacement queue."""

import argparse
import hashlib
import json
import pathlib
import stat
import sys


BASELINE = "07505ea58c7bba698d6b9220ff43946a43cac76b"
FIXED_COMPONENTS = (
    ("runtime-shared-libraries", "runtime", "lib"),
    ("package-common", "package-common", "static/packages/common"),
)
RUNTIME_COMPONENT_ROOTS = (
    ("tmp-update", "static/build/.tmp_update"),
    ("miyoo-app", "static/build/miyoo/app"),
    ("miyoo-lib", "static/build/miyoo/lib"),
)
RUNTIME_NESTED_COMPONENT_ROOTS = (
    ("tmp-update-bin", "static/build/.tmp_update/bin"),
    ("tmp-update-lib", "static/build/.tmp_update/lib"),
    ("tmp-update-res", "static/build/.tmp_update/res"),
    ("tmp-update-script", "static/build/.tmp_update/script"),
)
PACKAGE_KINDS = ("App", "Emu", "RApp")
ANNOTATION_FIELDS = ("source", "source_revision", "license", "build_recipe", "resolution")
RESOLUTIONS = ("replace-source-build-or-exclude", "source-build", "excluded")
ONION_SOURCE = "https://github.com/OnionUI/Onion"
ONION_WRAPPER_SUFFIXES = (".json", ".miyoocmd", ".notfound", ".sh")
ONION_RUNTIME_SUFFIXES = ("", ".json", ".reset", ".sh", ".tsv")
ONION_RUNTIME_SOURCE_IDS = {
    "runtime-tmp-update-res-miyoo283-system-json",
    "runtime-tmp-update-res-miyoo354-system-json",
    "runtime-tmp-update-res-wpa-supplicant-reset",
}
BLOOM_SOURCE = "https://github.com/boburning/BloomOS"
BLOOM_RUNTIME_SOURCE_IDS = {
    "runtime-miyoo-app-config-json",
    "runtime-miyoo-app-lang",
    "runtime-miyoo-lib--gitkeep",
    "runtime-tmp-update-config",
    "runtime-tmp-update-etc",
    "runtime-tmp-update-keys",
    "runtime-tmp-update-onionversion",
    "runtime-tmp-update-runtime-sh",
    "runtime-tmp-update-updater",
    "runtime-tmp-update-bin-bloom-detect-model",
    "runtime-tmp-update-bin-bloom-dev-ssh",
    "runtime-tmp-update-bin-bloom-diagnostics-export",
    "runtime-tmp-update-bin-bloom-controls",
    "runtime-tmp-update-bin-bloom-game-smoke",
    "runtime-tmp-update-bin-bloom-health-system",
    "runtime-tmp-update-bin-bloom-launch-override",
    "runtime-tmp-update-bin-bloom-launch-run",
    "runtime-tmp-update-bin-bloom-network",
    "runtime-tmp-update-bin-bloom-platform",
    "runtime-tmp-update-bin-bloom-power",
    "runtime-tmp-update-bin-bloom-ra-login",
    "runtime-tmp-update-bin-bloom-ra-session-prepare",
    "runtime-tmp-update-bin-bloom-ra-log",
    "runtime-tmp-update-bin-bloom-save-snapshot",
    "runtime-tmp-update-bin-bloom-session",
    "runtime-tmp-update-bin-bloom-test-runner",
    "runtime-tmp-update-bin-bloom-time",
    "runtime-tmp-update-bin-bloom-update-activate",
    "runtime-tmp-update-bin-bloom-update-boot",
    "runtime-tmp-update-bin-bloom-update-bootstrap",
    "runtime-tmp-update-bin-bloom-update-channel",
    "runtime-tmp-update-bin-bloom-update-prepare",
    "runtime-tmp-update-bin-bloom-update-rollback",
    "runtime-tmp-update-bin-bloom-update-stage",
    "runtime-tmp-update-bin-bloom-update-state",
    "runtime-tmp-update-bin-bloom-update-verify",
    "runtime-tmp-update-bin-bloomctl",
}
ADVANCEMENU_ID = "app-advancemenu--alternative-frontend"
OPENBOR_ID = "rapp-game-engine---open-beats-of-rage"
OPENBOR_SOURCE = "https://github.com/DCurrent/openbor"
OPENBOR_REVISION = "b00efbc7752cb55709dfc9fdfdfc7cfe78ddfb90"
OPENBOR_BINARY_SHA256 = "41ef99389f37a943eb67b9f50cb2847ff00c646f0f3d63598611684053b19c57"
PIXELREADER_ID = "app-ebook-reader--pixelreader"
PIXELREADER_SOURCE = "https://github.com/ealang/pixel-reader"
PIXELREADER_REVISION = "762ed8ee40bf24fc05af1b0df1a95d30acd56b5b"
PIXELREADER_BINARY_SHA256 = "08d7aa1f9a259becff7b8d3ef15a8bf296bda90dd74e15cf0d734c7229b04974"
SHARED_LIBRARIES_ID = "runtime-shared-libraries"
SHARED_LIBRARY_HASHES = {
    "libSDL_rotozoom.so": "6fd48052173aecb8e6b65c005c9e1c8027dc2fc6173f964703ee066762870a3a",
    "libsqlite3.so": "9e61ae98e776671f6a0cb5bd76faccdb78d61d4b5b2254ffd5bf0cb44d172bfa",
    "libsqlite3.so.0": "9e61ae98e776671f6a0cb5bd76faccdb78d61d4b5b2254ffd5bf0cb44d172bfa",
}
BATTERY_MONITOR_ID = "app-battery-monitor"
BATTERY_MONITOR_PACKAGE_ROOT = pathlib.PurePosixPath("App/BatteryMonitorUI")
BATTERY_MONITOR_SOURCE_ROOT = pathlib.PurePosixPath("src/batteryMonitorUI")
QUICK_GUIDE_ID = "app-quick-guide"
QUICK_GUIDE_ROOT = pathlib.PurePosixPath("App/Onion_Manual")
QUICK_GUIDE_HASHES = {
    "page1.png": "0c356f215b6ff0c41241a62d16d7d1176bbc0d431479dfd88af9c6dc6f79d41b",
    "page2.png": "823a2b8fb3ab438b7704b9e330b0364c5337b500eb03dc52176c4468e5d9acfa",
    "page3.png": "8f5e1f9c676c532361ded37a9fe18551a9488bab59c04c5faf1658278ac1e50b",
    "page4.png": "9b5dcd56b765a2c1fb0a598db061aaf6ca4e18bcfbdfcc362d4179e8ba5eac39",
}


def component_id(kind, name):
    value = "-".join((kind, name)).lower()
    return "".join(character if character.isalnum() else "-" for character in value).strip("-")


def component_specs(repository):
    specs = list(FIXED_COMPONENTS)
    nested_roots = {relative for _, relative in RUNTIME_NESTED_COMPONENT_ROOTS}
    for identifier_prefix, relative_root in RUNTIME_COMPONENT_ROOTS:
        root = repository / relative_root
        for child in sorted(root.iterdir(), key=lambda item: item.name.encode("utf-8")):
            if child.relative_to(repository).as_posix() in nested_roots:
                continue
            identifier = component_id("runtime", f"{identifier_prefix}-{child.name}")
            relative = child.relative_to(repository).as_posix()
            specs.append((identifier, "runtime", relative))
    for identifier_prefix, relative_root in RUNTIME_NESTED_COMPONENT_ROOTS:
        root = repository / relative_root
        for child in sorted(root.iterdir(), key=lambda item: item.name.encode("utf-8")):
            identifier = component_id("runtime", f"{identifier_prefix}-{child.name}")
            relative = child.relative_to(repository).as_posix()
            specs.append((identifier, "runtime", relative))
    package_root = repository / "static/packages"
    for kind in PACKAGE_KINDS:
        directory = package_root / kind
        for child in sorted(directory.iterdir(), key=lambda item: item.name.encode("utf-8")):
            if child.is_dir():
                relative = child.relative_to(repository).as_posix()
                specs.append((component_id(kind, child.name), "package", relative))
    return specs


def canonical_file(path):
    value = bytearray()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.extend(chunk)
    return bytes(value).replace(b"\r\n", b"\n")


def tree_identity(root):
    records = []
    byte_count = 0
    if root.is_dir():
        paths = sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix().encode("utf-8"))
    else:
        paths = (root,)
    for path in paths:
        relative = path.relative_to(root).as_posix() if root.is_dir() else path.name
        metadata = path.lstat()
        if stat.S_ISLNK(metadata.st_mode):
            content = path.readlink().as_posix().encode("utf-8")
            byte_count += len(content)
            records.append(("file", relative, len(content), hashlib.sha256(content).hexdigest()))
        elif stat.S_ISREG(metadata.st_mode):
            content = canonical_file(path)
            size = len(content)
            byte_count += size
            records.append(("file", relative, size, hashlib.sha256(content).hexdigest()))
        elif not stat.S_ISDIR(metadata.st_mode):
            raise ValueError(f"unsupported file type: {path}")
    if not records:
        raise ValueError(f"component has no files: {root}")
    encoded = json.dumps(records, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return len(records), byte_count, hashlib.sha256(encoded).hexdigest()


def make_component(repository, identifier, kind, relative):
    root = repository / relative
    if not root.exists() and not root.is_symlink():
        raise ValueError(f"missing component path: {relative}")
    file_count, byte_count, tree_sha256 = tree_identity(root)
    return {
        "id": identifier,
        "kind": kind,
        "path": relative,
        "file_count": file_count,
        "byte_count": byte_count,
        "tree_sha256": tree_sha256,
        "source": None,
        "source_revision": None,
        "license": None,
        "build_recipe": None,
        "resolution": "replace-source-build-or-exclude",
    }


def create_manifest(repository):
    components = [make_component(repository, *spec) for spec in component_specs(repository)]
    identifiers = [item["id"] for item in components]
    if len(identifiers) != len(set(identifiers)):
        raise ValueError("generated component ids are not unique")
    return {"schema": 1, "inherited_baseline": BASELINE, "components": components}


def canonical(value):
    return json.dumps(value, ensure_ascii=False, indent=2, sort_keys=False) + "\n"


def manifest_differences(current, generated):
    """Describe component inventory drift without dumping payload contents."""
    differences = []
    current_components = {
        item.get("id"): item
        for item in current.get("components", [])
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }
    generated_components = {item["id"]: item for item in generated["components"]}
    for identifier in sorted(set(current_components) | set(generated_components)):
        previous = current_components.get(identifier)
        replacement = generated_components.get(identifier)
        if previous is None:
            differences.append(f"{identifier}: added component")
            continue
        if replacement is None:
            differences.append(f"{identifier}: removed component")
            continue
        changed = [field for field in replacement if previous.get(field) != replacement.get(field)]
        if changed:
            differences.append(f"{identifier}: changed {', '.join(changed)}")
    if not differences and current != generated:
        differences.append("manifest formatting or top-level metadata differs")
    return differences


def apply_annotations(generated, current):
    existing = current.get("components")
    if not isinstance(existing, list):
        raise ValueError("legacy manifest components must be a list")
    annotations = {item.get("id"): item for item in existing if isinstance(item, dict)}
    if len(annotations) != len(existing):
        raise ValueError("legacy manifest component ids must be unique")
    for component in generated["components"]:
        previous = annotations.get(component["id"])
        if previous is None:
            continue
        for field in ANNOTATION_FIELDS:
            value = previous.get(field)
            if value is not None and not isinstance(value, str):
                raise ValueError(f"{component['id']}: {field} must be a string or null")
            component[field] = value
        resolution = component["resolution"]
        if resolution not in RESOLUTIONS:
            raise ValueError(f"{component['id']}: unknown resolution")
        if resolution == "source-build" and any(not component[field] for field in ANNOTATION_FIELDS[:4]):
            raise ValueError(f"{component['id']}: source-build resolution requires complete provenance")


def is_onion_source_wrapper(repository, component):
    if component["kind"] not in ("package", "package-common"):
        return False
    root = repository / component["path"]
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        content = canonical_file(path)
        try:
            content.decode("utf-8")
        except UnicodeDecodeError:
            return False
        if b"\0" in content:
            return False
        if path.suffix.lower() not in ONION_WRAPPER_SUFFIXES and content:
            return False
    return True


def annotate_onion_wrappers(repository, manifest):
    count = 0
    for component in manifest["components"]:
        if component["resolution"] != "replace-source-build-or-exclude":
            continue
        if not (
            is_onion_source_wrapper(repository, component)
            or is_onion_runtime_source(repository, component)
        ):
            continue
        component.update({
            "source": ONION_SOURCE,
            "source_revision": BASELINE,
            "license": "GPL-3.0-only",
            "build_recipe": "Makefile",
            "resolution": "source-build",
        })
        count += 1
    return count


def is_onion_runtime_source(repository, component):
    if component["kind"] != "runtime":
        return False
    root_path = pathlib.PurePosixPath(component["path"])
    script_root = pathlib.PurePosixPath("static/build/.tmp_update/script")
    if script_root not in root_path.parents and component["id"] not in ONION_RUNTIME_SOURCE_IDS:
        return False
    root = repository / component["path"]
    paths = root.rglob("*") if root.is_dir() else (root,)
    found = False
    for path in paths:
        if not path.is_file():
            continue
        found = True
        if path.suffix.lower() not in ONION_RUNTIME_SUFFIXES:
            return False
        content = canonical_file(path)
        if b"\0" in content:
            return False
        try:
            content.decode("utf-8")
        except UnicodeDecodeError:
            return False
    return found


def is_bloom_battery_monitor(repository, component):
    if component["id"] != BATTERY_MONITOR_ID or component["kind"] != "package":
        return False
    package_root = repository / component["path"] / BATTERY_MONITOR_PACKAGE_ROOT
    source_root = repository / BATTERY_MONITOR_SOURCE_ROOT
    expected_package_files = {
        "config.json",
        "launch.sh",
        "Makefile",
        "res/DejaVuSans.ttf",
        "res/DejaVu_LICENSE.txt",
        "res/background.png",
        "res/end.png",
        "res/left_arrow.png",
        "res/right_arrow.png",
        "res/waiting_screen.png",
    }
    actual_package_files = {
        path.relative_to(package_root).as_posix()
        for path in package_root.rglob("*")
        if path.is_file()
    }
    if actual_package_files != expected_package_files:
        return False
    for relative in ("Makefile",) + tuple(
        sorted(item for item in expected_package_files if item.startswith("res/"))
    ):
        packaged = package_root / relative
        source = source_root / relative
        if not source.is_file() or canonical_file(packaged) != canonical_file(source):
            return False
    source_code = source_root / "batteryMonitorUI.c"
    if not source_code.is_file() or b'TTF_OpenFont("./res/DejaVuSans.ttf", 15)' not in canonical_file(source_code):
        return False
    return True


def is_bloom_runtime_source(repository, component):
    if component["id"] not in BLOOM_RUNTIME_SOURCE_IDS or component["kind"] != "runtime":
        return False
    root = repository / component["path"]
    paths = root.rglob("*") if root.is_dir() else (root,)
    found = False
    for path in paths:
        if not path.is_file():
            continue
        found = True
        content = canonical_file(path)
        if b"\0" in content:
            return False
        try:
            content.decode("utf-8")
        except UnicodeDecodeError:
            return False
    return found


def is_bloom_quick_guide(repository, component):
    if component["id"] != QUICK_GUIDE_ID or component["kind"] != "package":
        return False
    package_root = repository / component["path"] / QUICK_GUIDE_ROOT
    expected_files = {"config.json", "images.json", "launch.sh", *QUICK_GUIDE_HASHES}
    actual_files = {
        path.relative_to(package_root).as_posix()
        for path in package_root.rglob("*")
        if path.is_file()
    }
    if actual_files != expected_files:
        return False
    if b'"description":"Getting started with BloomOS"' not in canonical_file(package_root / "config.json"):
        return False
    if b"infoPanel --images-json /mnt/SDCARD/App/Onion_Manual/images.json" not in canonical_file(package_root / "launch.sh"):
        return False
    generator = repository / "tools/generate_quick_guide.py"
    if not generator.is_file():
        return False
    for name, expected in QUICK_GUIDE_HASHES.items():
        if hashlib.sha256((package_root / name).read_bytes()).hexdigest() != expected:
            return False
    return True


def is_bloom_advancemenu_wrapper(repository, component):
    if component["id"] != ADVANCEMENU_ID or component["kind"] != "package":
        return False
    root = repository / component["path"]
    expected_files = {
        "App/AdvanceMENU/config.json",
        "App/AdvanceMENU/info.txt",
        "App/AdvanceMENU/launch.sh",
        "App/romscripts/emu/Open AdvanceMENU.sh",
        "App/romscripts/rapp/Open AdvanceMENU.sh",
    }
    files = {
        path.relative_to(root).as_posix(): path
        for path in root.rglob("*")
        if path.is_file()
    }
    if set(files) != expected_files:
        return False
    for path in files.values():
        content = canonical_file(path)
        try:
            content.decode("utf-8")
        except UnicodeDecodeError:
            return False
        if b"\0" in content:
            return False
    launch = canonical_file(files["App/AdvanceMENU/launch.sh"])
    return b"cd $sysdir/bin/adv" in launch and b"./run_advmenu.sh" in launch


def is_source_built_openbor(repository, component):
    if component["id"] != OPENBOR_ID or component["kind"] != "package":
        return False
    root = repository / component["path"]
    expected_files = {
        "RApp/OpenBOR/LICENSE.MD",
        "RApp/OpenBOR/OpenBOR",
        "RApp/OpenBOR/config.json",
        "RApp/OpenBOR/launch.sh",
        "Roms/OPENBOR/.gitkeep",
    }
    files = {
        path.relative_to(root).as_posix(): path
        for path in root.rglob("*")
        if path.is_file()
    }
    if set(files) != expected_files:
        return False
    binary_hash = hashlib.sha256(files["RApp/OpenBOR/OpenBOR"].read_bytes()).hexdigest()
    if binary_hash != OPENBOR_BINARY_SHA256:
        return False
    build_script = canonical_file(repository / "build/openbor/build.sh")
    return b"third-party/openbor" in build_script and b"openbor-mmiyoo.patch" in build_script


def is_source_built_pixelreader(repository, component):
    if component["id"] != PIXELREADER_ID or component["kind"] != "package":
        return False
    root = repository / component["path"]
    expected_files = {
        "App/PixelReader/LICENSE.PixelReader",
        "App/PixelReader/LICENSE.libxml2",
        "App/PixelReader/LICENSE.libzip",
        "App/PixelReader/LICENSE.zlib",
        "App/PixelReader/config.json",
        "App/PixelReader/launch.sh",
        "App/PixelReader/reader",
        "App/PixelReader/reader.cfg",
        "App/PixelReader/resources/fonts/DejaVu_License.txt",
        "App/PixelReader/resources/fonts/DejaVuSans.ttf",
        "App/PixelReader/resources/fonts/DejaVuSansMono.ttf",
        "App/PixelReader/resources/fonts/DejaVuSerif.ttf",
    }
    files = {
        path.relative_to(root).as_posix(): path
        for path in root.rglob("*")
        if path.is_file()
    }
    if set(files) != expected_files:
        return False
    if hashlib.sha256(files["App/PixelReader/reader"].read_bytes()).hexdigest() != PIXELREADER_BINARY_SHA256:
        return False
    license_sources = {
        "App/PixelReader/LICENSE.PixelReader": repository / "third-party/pixel-reader/LICENSE",
        "App/PixelReader/LICENSE.zlib": repository / "third-party/zlib/LICENSE",
        "App/PixelReader/LICENSE.libxml2": repository / "third-party/libxml2/Copyright",
        "App/PixelReader/LICENSE.libzip": repository / "third-party/libzip/LICENSE",
    }
    for relative, source in license_sources.items():
        if not source.is_file() or canonical_file(files[relative]) != canonical_file(source):
            return False
    source_fonts = repository / "third-party/pixel-reader/resources/fonts"
    for relative in expected_files:
        if "/resources/fonts/" not in relative:
            continue
        if canonical_file(files[relative]) != canonical_file(source_fonts / pathlib.PurePosixPath(relative).name):
            return False
    build_script = canonical_file(repository / "build/pixelreader/build.sh")
    return all(
        marker in build_script
        for marker in (b"third-party/pixel-reader", b"third-party/zlib", b"third-party/libxml2", b"third-party/libzip")
    )


def is_source_built_shared_libraries(repository, component):
    if component["id"] != SHARED_LIBRARIES_ID or component["kind"] != "runtime":
        return False
    root = repository / component["path"]
    files = {
        path.relative_to(root).as_posix(): path
        for path in root.rglob("*")
        if path.is_file()
    }
    if set(files) != set(SHARED_LIBRARY_HASHES):
        return False
    for relative, expected_hash in SHARED_LIBRARY_HASHES.items():
        if hashlib.sha256(files[relative].read_bytes()).hexdigest() != expected_hash:
            return False
    build_script = canonical_file(repository / "build/shared-libs/build.sh")
    return all(
        marker in build_script
        for marker in (b"include/sqlite3/sqlite3.c", b"include/SDL/SDL_rotozoom.c", b"--build-id=none")
    )


def annotate_bloom_replacements(repository, manifest):
    count = 0
    for component in manifest["components"]:
        if component["resolution"] != "replace-source-build-or-exclude":
            continue
        if is_bloom_runtime_source(repository, component):
            component.update({
                "source": BLOOM_SOURCE,
                "source_revision": "release-commit",
                "license": "GPL-3.0-only",
                "build_recipe": "Makefile",
                "resolution": "source-build",
            })
        elif is_bloom_battery_monitor(repository, component):
            component.update({
                "source": BLOOM_SOURCE,
                "source_revision": "release-commit",
                "license": "GPL-3.0-only AND LicenseRef-DejaVu-Fonts",
                "build_recipe": "Makefile",
                "resolution": "source-build",
            })
        elif is_bloom_quick_guide(repository, component):
            component.update({
                "source": BLOOM_SOURCE,
                "source_revision": "release-commit",
                "license": "GPL-3.0-only",
                "build_recipe": "tools/generate_quick_guide.py",
                "resolution": "source-build",
            })
        elif is_bloom_advancemenu_wrapper(repository, component):
            component.update({
                "source": BLOOM_SOURCE,
                "source_revision": "release-commit",
                "license": "GPL-3.0-only",
                "build_recipe": "Makefile",
                "resolution": "source-build",
            })
        elif is_source_built_openbor(repository, component):
            component.update({
                "source": OPENBOR_SOURCE,
                "source_revision": OPENBOR_REVISION,
                "license": "BSD-3-Clause",
                "build_recipe": "build/openbor/build.sh",
                "resolution": "source-build",
            })
        elif is_source_built_pixelreader(repository, component):
            component.update({
                "source": PIXELREADER_SOURCE,
                "source_revision": PIXELREADER_REVISION,
                "license": "GPL-3.0-only AND Zlib AND MIT AND BSD-3-Clause AND LicenseRef-DejaVu-Fonts",
                "build_recipe": "build/pixelreader/build.sh",
                "resolution": "source-build",
            })
        elif is_source_built_shared_libraries(repository, component):
            component.update({
                "source": BLOOM_SOURCE,
                "source_revision": "release-commit",
                "license": "LicenseRef-SQLite-Public-Domain AND LGPL-2.0-or-later",
                "build_recipe": "build/shared-libs/build.sh",
                "resolution": "source-build",
            })
        else:
            continue
        count += 1
    return count


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "command",
        choices=("create", "validate", "annotate-onion-wrappers", "annotate-bloom-replacements"),
    )
    parser.add_argument("--repository", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        current = None
        if args.manifest.exists():
            current = args.manifest.read_text(encoding="utf-8")
            parsed = json.loads(current)
            if parsed.get("schema") != 1:
                raise ValueError("unsupported legacy manifest schema")
        elif args.command != "create":
            raise ValueError(f"legacy manifest does not exist: {args.manifest}")
        generated = create_manifest(args.repository)
        if current is not None:
            apply_annotations(generated, parsed)
        rendered = canonical(generated)
        if args.command == "annotate-onion-wrappers":
            count = annotate_onion_wrappers(args.repository, generated)
            args.manifest.write_text(canonical(generated), encoding="utf-8", newline="\n")
            print(f"legacy manifest annotated: {count} Onion source wrappers")
            return 0
        if args.command == "annotate-bloom-replacements":
            count = annotate_bloom_replacements(args.repository, generated)
            args.manifest.write_text(canonical(generated), encoding="utf-8", newline="\n")
            print(f"legacy manifest annotated: {count} Bloom source replacements")
            return 0
        if args.command == "create":
            args.manifest.write_text(rendered, encoding="utf-8", newline="\n")
        elif current != rendered:
            detail = "; ".join(manifest_differences(parsed, generated))
            raise ValueError(f"legacy manifest is stale or non-canonical: {detail}")
        print(f"legacy manifest {args.command}: {len(generated['components'])} components")
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
