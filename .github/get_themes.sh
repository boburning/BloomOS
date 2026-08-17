#!/bin/bash

set -euo pipefail

readonly THEMES_REVISION="b01198352e8927c3c5b9a828f73177bc81745954"
readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly LOCK_FILE="$ROOT_DIR/build/themes.sha256"
readonly CACHE_DIR="$ROOT_DIR/cache/themes/$THEMES_REVISION"
readonly DIST_DIR="$ROOT_DIR/dist/Themes"
readonly BASE_URL="https://raw.githubusercontent.com/OnionUI/Themes/$THEMES_REVISION/release"
readonly DOWNLOAD_ATTEMPTS=8
readonly DOWNLOAD_TIMEOUT=20

download_tmp=""

cleanup_download() {
    [[ -z "$download_tmp" ]] || rm -f "$download_tmp"
}

download_theme() {
    local url="$1"
    local destination="$2"
    local attempt

    download_tmp="${destination}.part.$$"
    for ((attempt = 1; attempt <= DOWNLOAD_ATTEMPTS; attempt++)); do
        if wget -q --show-progress --tries=1 --timeout="$DOWNLOAD_TIMEOUT" \
            -O "$download_tmp" "$url"; then
            mv "$download_tmp" "$destination"
            download_tmp=""
            return 0
        fi

        rm -f "$download_tmp"
        if ((attempt < DOWNLOAD_ATTEMPTS)); then
            echo "Theme download failed; retrying ($attempt/$DOWNLOAD_ATTEMPTS)..." >&2
            sleep "$((attempt * 2))"
        fi
    done

    echo "Theme download failed after $DOWNLOAD_ATTEMPTS attempts: ${destination##*/}" >&2
    download_tmp=""
    return 1
}

trap cleanup_download EXIT HUP INT TERM

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
        download_theme "$BASE_URL/$filename" "$zipfile"
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
