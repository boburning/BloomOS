#!/bin/sh
set -eu

git config --global --add safe.directory "$(pwd)"

for path in "$@"; do
    case "$path" in
        third-party/*) ;;
        *)
            printf 'Invalid submodule path: %s\n' "$path" >&2
            exit 1
            ;;
    esac
done

if [ "$#" -eq 0 ]; then
    git submodule sync --recursive
    git submodule init
else
    git submodule sync --recursive -- "$@"
    git submodule init -- "$@"
fi

attempt=1
while true; do
    if [ "$#" -eq 0 ]; then
        git -c protocol.version=2 submodule update --init --force --depth 1 --recursive && break
    else
        git -c protocol.version=2 submodule update --init --force --depth 1 --recursive -- "$@" && break
    fi
    if [ "$attempt" -ge 4 ]; then
        printf 'Pinned submodule checkout failed after %s attempts\n' "$attempt" >&2
        exit 1
    fi

    delay=$((attempt * 10))
    printf 'Pinned submodule checkout attempt %s failed; retrying in %ss\n' "$attempt" "$delay" >&2
    sleep "$delay"
    attempt=$((attempt + 1))
done
