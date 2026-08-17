#!/usr/bin/env python3
"""Create and validate BloomOS release metadata using only the standard library."""

from __future__ import annotations

import argparse
import hashlib
import json
import stat
from datetime import datetime, timezone
from pathlib import Path
from zipfile import BadZipFile, ZipFile


COMPATIBLE_DEVICES = ["miyoo-mini-v1", "miyoo-mini-v2", "miyoo-mini-v3", "miyoo-mini-v4", "miyoo-mini-plus", "miyoo-mini-flip"]
MANIFEST_DEVICES = ["mini", "plus", "flip"]
REQUIRED_ARCHIVE_PATHS = {"miyoo/app/.tmp_update/onion.pak", "RetroArch/retroarch.pak"}
RELEASE_CHANNELS = {"stable", "beta", "nightly", "development"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def payload_manifest(archive: Path, archive_hash: str) -> dict:
    files: list[dict] = []
    seen: set[str] = set()
    try:
        with ZipFile(archive) as release_zip:
            for member in release_zip.infolist():
                path = member.filename[2:] if member.filename.startswith("./") else member.filename
                parts = Path(path).parts
                if not path or path.startswith("/") or ".." in parts or "\\" in path:
                    raise SystemExit(f"release archive contains unsafe path: {member.filename}")
                if path in seen:
                    raise SystemExit(f"release archive contains duplicate path: {path}")
                seen.add(path)
                if member.is_dir():
                    continue
                data = release_zip.read(member)
                mode = member.external_attr >> 16
                file_type = "symlink" if stat.S_ISLNK(mode) else "file"
                files.append(
                    {
                        "path": path,
                        "type": file_type,
                        "size": len(data),
                        "sha256": hashlib.sha256(data).hexdigest(),
                    }
                )
    except BadZipFile as error:
        raise SystemExit(f"release archive is invalid: {error}") from error
    files.sort(key=lambda item: item["path"])
    return {"schema": 1, "product": "BloomOS", "archive_sha256": archive_hash, "files": files}


def create(args: argparse.Namespace) -> None:
    output_dir = Path(args.output_dir).resolve()
    archive = Path(args.archive).resolve()
    dependency_lock = Path(args.dependency_lock).resolve()
    if archive.parent != output_dir or not archive.is_file():
        raise SystemExit("archive must exist directly inside the output directory")
    if not dependency_lock.is_file():
        raise SystemExit("dependency lock does not exist")

    archive_hash = sha256(archive)
    payload = payload_manifest(archive, archive_hash)
    write_json(output_dir / "payload-manifest.json", payload)
    build_date = datetime.fromtimestamp(args.source_date_epoch, timezone.utc).isoformat().replace("+00:00", "Z")
    write_json(
        output_dir / "build-info.json",
        {
            "schema": 1,
            "product": "BloomOS",
            "version": args.version,
            "channel": args.channel,
            "git_commit": args.commit,
            "build_date": build_date,
            "source_date_epoch": args.source_date_epoch,
            "toolchain": args.toolchain,
            "dependency_lock_sha256": sha256(dependency_lock),
            "payload_manifest_sha256": sha256(output_dir / "payload-manifest.json"),
            "compatible_devices": COMPATIBLE_DEVICES,
        },
    )
    write_json(
        output_dir / "manifest.json",
        {
            "schema": 1,
            "product": "BloomOS",
            "version": args.version,
            "channel": args.channel,
            "devices": MANIFEST_DEVICES,
            "artifacts": [
                {
                    "filename": archive.name,
                    "sha256": archive_hash,
                    "size": archive.stat().st_size,
                    "media_type": "application/zip",
                }
            ],
        },
    )
    (output_dir / "SHA256SUMS").write_text(f"{archive_hash}  {archive.name}\n", encoding="ascii")


def load_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"invalid {path.name}: {error}") from error
    if not isinstance(value, dict):
        raise SystemExit(f"invalid {path.name}: root must be an object")
    return value


def validate(args: argparse.Namespace) -> None:
    output_dir = Path(args.output_dir).resolve()
    signature = output_dir / "manifest.sig"
    if not signature.is_file() or signature.stat().st_size != 64:
        raise SystemExit("manifest signature is missing or invalid")
    manifest = load_json(output_dir / "manifest.json")
    build_info = load_json(output_dir / "build-info.json")
    recorded_payload = load_json(output_dir / "payload-manifest.json")
    artifacts = manifest.get("artifacts")
    if manifest.get("product") != "BloomOS" or build_info.get("product") != "BloomOS":
        raise SystemExit("release metadata has the wrong product")
    if manifest.get("version") != build_info.get("version"):
        raise SystemExit("release metadata versions do not match")
    if manifest.get("channel") not in RELEASE_CHANNELS or manifest.get("channel") != build_info.get("channel"):
        raise SystemExit("release metadata channels are invalid or do not match")
    if manifest.get("devices") != MANIFEST_DEVICES:
        raise SystemExit("manifest device targets are invalid")
    if not isinstance(artifacts, list) or len(artifacts) != 1 or not isinstance(artifacts[0], dict):
        raise SystemExit("manifest must describe exactly one release archive")

    artifact = artifacts[0]
    archive = output_dir / str(artifact.get("filename", ""))
    if archive.parent != output_dir or not archive.is_file():
        raise SystemExit("manifest archive is missing or outside the release directory")
    actual_hash = sha256(archive)
    if artifact.get("sha256") != actual_hash or artifact.get("size") != archive.stat().st_size:
        raise SystemExit("manifest archive digest or size does not match")
    expected_sums = f"{actual_hash}  {archive.name}\n"
    if (output_dir / "SHA256SUMS").read_text(encoding="ascii") != expected_sums:
        raise SystemExit("SHA256SUMS does not match the release archive")
    actual_payload = payload_manifest(archive, actual_hash)
    if recorded_payload != actual_payload:
        raise SystemExit("payload manifest does not match the release archive")
    if build_info.get("payload_manifest_sha256") != sha256(output_dir / "payload-manifest.json"):
        raise SystemExit("build info payload manifest digest does not match")

    try:
        with ZipFile(archive) as release_zip:
            names = {name[2:] if name.startswith("./") else name for name in release_zip.namelist()}
            bad_member = release_zip.testzip()
    except BadZipFile as error:
        raise SystemExit(f"release archive is invalid: {error}") from error
    if bad_member:
        raise SystemExit(f"release archive contains a corrupt member: {bad_member}")
    missing = sorted(REQUIRED_ARCHIVE_PATHS - names)
    if missing:
        raise SystemExit(f"release archive is missing required paths: {', '.join(missing)}")


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    subparsers = root.add_subparsers(dest="command", required=True)
    create_parser = subparsers.add_parser("create")
    create_parser.add_argument("--output-dir", required=True)
    create_parser.add_argument("--archive", required=True)
    create_parser.add_argument("--version", required=True)
    create_parser.add_argument("--channel", required=True)
    create_parser.add_argument("--commit", required=True)
    create_parser.add_argument("--source-date-epoch", required=True, type=int)
    create_parser.add_argument("--dependency-lock", required=True)
    create_parser.add_argument("--toolchain", required=True)
    create_parser.set_defaults(handler=create)
    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("--output-dir", required=True)
    validate_parser.set_defaults(handler=validate)
    return root


def main() -> None:
    args = parser().parse_args()
    args.handler(args)


if __name__ == "__main__":
    main()
