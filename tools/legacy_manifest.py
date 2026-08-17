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
    ("runtime-tmp-update", "runtime", "static/build/.tmp_update"),
    ("runtime-miyoo", "runtime", "static/build/miyoo"),
    ("runtime-shared-libraries", "runtime", "lib"),
    ("package-common", "package-common", "static/packages/common"),
)
PACKAGE_KINDS = ("App", "Emu", "RApp")
ANNOTATION_FIELDS = ("source", "source_revision", "license", "build_recipe", "resolution")
RESOLUTIONS = ("replace-source-build-or-exclude", "source-build", "excluded")
ONION_SOURCE = "https://github.com/OnionUI/Onion"
ONION_WRAPPER_SUFFIXES = (".json", ".miyoocmd", ".notfound", ".sh")


def component_id(kind, name):
    value = "-".join((kind, name)).lower()
    return "".join(character if character.isalnum() else "-" for character in value).strip("-")


def component_specs(repository):
    specs = list(FIXED_COMPONENTS)
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
    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix().encode("utf-8")):
        relative = path.relative_to(root).as_posix()
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
    if not root.is_dir():
        raise ValueError(f"missing component directory: {relative}")
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
        if not is_onion_source_wrapper(repository, component):
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("create", "validate", "annotate-onion-wrappers"))
    parser.add_argument("--repository", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    args = parser.parse_args()
    try:
        current = None
        if args.command != "create":
            current = args.manifest.read_text(encoding="utf-8")
            parsed = json.loads(current)
            if parsed.get("schema") != 1:
                raise ValueError("unsupported legacy manifest schema")
        generated = create_manifest(args.repository)
        if current is not None:
            apply_annotations(generated, parsed)
        rendered = canonical(generated)
        if args.command == "annotate-onion-wrappers":
            count = annotate_onion_wrappers(args.repository, generated)
            args.manifest.write_text(canonical(generated), encoding="utf-8", newline="\n")
            print(f"legacy manifest annotated: {count} Onion source wrappers")
            return 0
        if args.command == "create":
            args.manifest.write_text(rendered, encoding="utf-8", newline="\n")
        elif current != rendered:
            raise ValueError("legacy manifest is stale or non-canonical")
        print(f"legacy manifest {args.command}: {len(generated['components'])} components")
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
