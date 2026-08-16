#!/bin/bash

set -euo pipefail

readonly THEMES_REVISION="b01198352e8927c3c5b9a828f73177bc81745954"
readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly LOCK_FILE="$ROOT_DIR/build/themes.sha256"
readonly CACHE_DIR="$ROOT_DIR/cache/themes/$THEMES_REVISION"
readonly DIST_DIR="$ROOT_DIR/dist/Themes"
readonly BASE_URL="https://raw.githubusercontent.com/OnionUI/Themes/$THEMES_REVISION/release"

mkdir -p "$CACHE_DIR" "$DIST_DIR"

while IFS= read -r entry || [[ -n "$entry" ]]; do
    [[ -z "$entry" || "$entry" == \#* ]] && continue

    expected_hash="${entry%%  *}"
    filename="${entry#*  }"
    zipfile="$CACHE_DIR/$filename"

    if [[ -f "$zipfile" ]] && ! printf '%s  %s\n' "$expected_hash" "$zipfile" | sha256sum --check --status; then
        rm -f "$zipfile"
    fi

    if [[ ! -f "$zipfile" ]]; then
        echo "-- downloading pinned theme: ${filename%.zip}"
        wget -q --show-progress -O "$zipfile" "$BASE_URL/$filename"
    fi

    printf '%s  %s\n' "$expected_hash" "$zipfile" | sha256sum --check --status || {
        echo "Theme checksum verification failed: $filename" >&2
        rm -f "$zipfile"
        exit 1
    }

    if [[ "$filename" == "Silky by DiMo.zip" ]]; then
        echo "-- extracting theme: ${filename%.zip}"
        unzip -oq "$zipfile" -d "$DIST_DIR"
    else
        echo "-- copying theme: ${filename%.zip}"
        cp "$zipfile" "$DIST_DIR"
    fi
done <"$LOCK_FILE"
