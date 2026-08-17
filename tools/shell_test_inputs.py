#!/usr/bin/env python3
"""Verify and safely extract the archived shell-test APK closure."""

import argparse
import hashlib
import json
import pathlib
import re
import tarfile


SHA256_LINE = re.compile(r"^([0-9a-f]{64})  ([A-Za-z0-9_.+~-]+\.apk)$")


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--metadata", type=pathlib.Path, required=True)
    parser.add_argument("--archive", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    metadata = json.loads(args.metadata.read_text(encoding="utf-8"))
    if metadata.get("schema") != 1:
        raise SystemExit("unsupported shell-test input schema")
    if args.archive.name != metadata.get("archive"):
        raise SystemExit("shell-test input archive name mismatch")
    if args.archive.stat().st_size != metadata.get("archive_size"):
        raise SystemExit("shell-test input archive size mismatch")
    if digest(args.archive) != metadata.get("archive_sha256"):
        raise SystemExit("shell-test input archive digest mismatch")

    with tarfile.open(args.archive, "r:gz") as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        required_metadata = {"SHA256SUMS", "PACKAGE-LICENSES"}
        if len(names) != len(set(names)) or not required_metadata.issubset(names):
            raise SystemExit("shell-test input archive has a missing or duplicate manifest")
        if any(not member.isfile() or "/" in member.name or "\\" in member.name for member in members):
            raise SystemExit("shell-test input archive contains an unsafe member")
        manifest = archive.extractfile("SHA256SUMS").read().decode("ascii")
        if hashlib.sha256(manifest.encode("ascii")).hexdigest() != metadata.get("package_manifest_sha256"):
            raise SystemExit("shell-test package manifest digest mismatch")
        expected = {}
        for line in manifest.splitlines():
            match = SHA256_LINE.fullmatch(line)
            if not match or match.group(2) in expected:
                raise SystemExit("shell-test package manifest is malformed")
            expected[match.group(2)] = match.group(1)
        package_names = sorted(name for name in names if name.endswith(".apk"))
        if package_names != sorted(expected) or len(package_names) != metadata.get("package_count"):
            raise SystemExit("shell-test package inventory mismatch")
        if set(names) != set(package_names) | required_metadata:
            raise SystemExit("shell-test input archive contains an unexpected member")
        licenses = archive.extractfile("PACKAGE-LICENSES").read().decode("utf-8")
        if hashlib.sha256(licenses.encode("utf-8")).hexdigest() != metadata.get("package_licenses_sha256"):
            raise SystemExit("shell-test package license manifest digest mismatch")
        licensed = {}
        for line in licenses.splitlines():
            name, separator, license_value = line.partition("\t")
            if not separator or not license_value or name in licensed:
                raise SystemExit("shell-test package license manifest is malformed")
            licensed[name] = license_value
        if sorted(licensed) != package_names:
            raise SystemExit("shell-test package license inventory mismatch")

        args.output.mkdir(parents=True, exist_ok=False)
        for member in members:
            source = archive.extractfile(member)
            target = args.output / member.name
            with target.open("wb") as destination:
                for chunk in iter(lambda: source.read(1024 * 1024), b""):
                    destination.write(chunk)

    for name, expected_digest in expected.items():
        if digest(args.output / name) != expected_digest:
            raise SystemExit(f"shell-test package digest mismatch: {name}")
    print(f"shell-test inputs verified: {len(expected)} packages")


if __name__ == "__main__":
    main()
