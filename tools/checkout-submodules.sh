#!/bin/sh
set -eu

git config --global --add safe.directory "$(pwd)"
git submodule sync --recursive
git submodule init

attempt=1
while ! git -c protocol.version=2 submodule update --init --force --depth 1 --recursive; do
    if [ "$attempt" -ge 4 ]; then
        printf 'Pinned submodule checkout failed after %s attempts\n' "$attempt" >&2
        exit 1
    fi

    delay=$((attempt * 10))
    printf 'Pinned submodule checkout attempt %s failed; retrying in %ss\n' "$attempt" "$delay" >&2
    sleep "$delay"
    attempt=$((attempt + 1))
done
