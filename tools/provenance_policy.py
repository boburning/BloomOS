#!/usr/bin/env python3
"""Validate BloomOS component provenance and release-channel eligibility."""

import argparse
import json
import pathlib
import sys


SOURCE_FIELDS = ("source", "source_revision", "license", "build_recipe")
LEGACY_FIELDS = ("reason",)
INVENTORY_RESOLUTIONS = ("replace-source-build-or-exclude", "source-build", "excluded")


def load_policy(path):
    try:
        policy = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read provenance policy: {error}") from error
    if policy.get("schema") != 1:
        raise ValueError("unsupported provenance policy schema")
    channels = policy.get("channels")
    tiers = policy.get("tiers")
    components = policy.get("components")
    if not isinstance(channels, list) or not channels or len(channels) != len(set(channels)):
        raise ValueError("channels must be a non-empty unique list")
    if not isinstance(tiers, dict) or not isinstance(components, list) or not components:
        raise ValueError("tiers and components are required")

    identifiers = set()
    covered_paths = set()
    for component in components:
        if not isinstance(component, dict):
            raise ValueError("every component must be an object")
        identifier = component.get("id")
        tier = component.get("tier")
        paths = component.get("paths")
        if not isinstance(identifier, str) or not identifier or identifier in identifiers:
            raise ValueError("component ids must be non-empty and unique")
        identifiers.add(identifier)
        if tier not in tiers:
            raise ValueError(f"{identifier}: unknown tier")
        if not isinstance(paths, list) or not paths or not all(isinstance(item, str) and item for item in paths):
            raise ValueError(f"{identifier}: paths must be a non-empty string list")
        for item in paths:
            if item in covered_paths:
                raise ValueError(f"{identifier}: duplicate covered path: {item}")
            covered_paths.add(item)
        required = SOURCE_FIELDS if tier == "source" else LEGACY_FIELDS if tier == "legacy" else ()
        for field in required:
            if not isinstance(component.get(field), str) or not component[field]:
                raise ValueError(f"{identifier}: {field} is required for tier {tier}")
        license_file = component.get("license_file")
        if license_file is not None and (not isinstance(license_file, str) or not license_file):
            raise ValueError(f"{identifier}: license_file must be a non-empty string")
        inventory_kinds = component.get("inventory_kinds")
        if tier == "inventory":
            if not isinstance(component.get("inventory"), str) or not component["inventory"]:
                raise ValueError(f"{identifier}: inventory is required for tier inventory")
            if (not isinstance(inventory_kinds, list) or not inventory_kinds or
                    not all(isinstance(item, str) and item for item in inventory_kinds) or
                    len(inventory_kinds) != len(set(inventory_kinds))):
                raise ValueError(f"{identifier}: inventory_kinds must be a non-empty unique string list")
        elif inventory_kinds is not None:
            raise ValueError(f"{identifier}: inventory_kinds requires tier inventory")

    for tier, settings in tiers.items():
        allowed = settings.get("allowed_channels") if isinstance(settings, dict) else None
        if not isinstance(allowed, list) or any(channel not in channels for channel in allowed):
            raise ValueError(f"{tier}: allowed_channels must reference declared channels")
    return policy


def check_files(policy, repository):
    for component in policy["components"]:
        for relative in component["paths"]:
            if not (repository / relative).exists():
                raise ValueError(f"{component['id']}: missing covered path: {relative}")
        for field in ("license_file", "inventory", "build_recipe"):
            relative = component.get(field)
            if relative and not (repository / relative).is_file():
                raise ValueError(f"{component['id']}: missing {field}: {relative}")


def load_inventory(path):
    try:
        inventory = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read component inventory: {error}") from error
    components = inventory.get("components") if inventory.get("schema") == 1 else None
    if not isinstance(components, list) or not components:
        raise ValueError(f"invalid component inventory: {path}")
    identifiers = set()
    for component in components:
        identifier = component.get("id") if isinstance(component, dict) else None
        kind = component.get("kind") if isinstance(component, dict) else None
        resolution = component.get("resolution") if isinstance(component, dict) else None
        if not isinstance(identifier, str) or not identifier or identifier in identifiers:
            raise ValueError(f"invalid or duplicate component inventory id: {identifier}")
        if not isinstance(kind, str) or not kind:
            raise ValueError(f"{identifier}: component inventory kind is required")
        if resolution not in INVENTORY_RESOLUTIONS:
            raise ValueError(f"{identifier}: invalid component inventory resolution")
        identifiers.add(identifier)
    return components


def inventory_status(policy, repository):
    selected = {}
    inventories = {}
    for policy_component in policy["components"]:
        if policy_component["tier"] != "inventory":
            continue
        relative = policy_component["inventory"]
        if relative not in inventories:
            inventories[relative] = load_inventory(repository / relative)
        inventory = inventories[relative]
        for component in inventory:
            if component["kind"] not in policy_component["inventory_kinds"]:
                continue
            identifier = component["id"]
            if identifier in selected:
                raise ValueError(f"{identifier}: selected by multiple inventory policy components")
            selected[identifier] = (policy_component["id"], component["resolution"])
    for relative, inventory in inventories.items():
        missing = [component["id"] for component in inventory if component["id"] not in selected]
        if missing:
            raise ValueError(f"component inventory entries are not covered by policy: {', '.join(missing)}")
    return selected


def check_channel(policy, channel, repository):
    if channel not in policy["channels"]:
        raise ValueError(f"unknown release channel: {channel}")
    blocked = []
    inventory_entries = inventory_status(policy, repository)
    for component in policy["components"]:
        if component["tier"] == "inventory":
            continue
        allowed = policy["tiers"][component["tier"]]["allowed_channels"]
        if channel not in allowed:
            blocked.append(f"{component['id']} ({component['tier']})")
    for identifier, (_, resolution) in inventory_entries.items():
        if resolution == "excluded":
            blocked.append(f"{identifier} (excluded inventory)")
        elif resolution != "source-build" and channel != "development":
            blocked.append(f"{identifier} (unresolved inventory)")
    if blocked:
        raise ValueError(f"{channel} release blocked by provenance policy: {', '.join(blocked)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("validate", "check-channel"))
    parser.add_argument("--policy", type=pathlib.Path, required=True)
    parser.add_argument("--repository", type=pathlib.Path, required=True)
    parser.add_argument("--channel")
    parser.add_argument("--require-files", action="store_true")
    args = parser.parse_args()
    try:
        policy = load_policy(args.policy)
        if args.require_files:
            check_files(policy, args.repository)
        if args.command == "check-channel":
            if args.channel is None:
                raise ValueError("--channel is required")
            check_channel(policy, args.channel, args.repository)
        print(f"provenance policy {args.command}: {len(policy['components'])} components")
    except ValueError as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
