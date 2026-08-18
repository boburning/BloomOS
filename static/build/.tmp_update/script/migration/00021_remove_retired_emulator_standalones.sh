#!/bin/sh

# BloomOS retains the maintained libretro cores for these systems. Remove the
# older optional standalone packages when upgrading an existing card so that
# excluded catalog entries do not remain launchable indefinitely.
for old_dir in \
    "/mnt/SDCARD/RApp/PICO/FAKE08" \
    "/mnt/SDCARD/RApp/PCSX-ReARMed"
do
    if [ -d "$old_dir" ] && [ ! -L "$old_dir" ]; then
        rm -rf "$old_dir"
    fi
done
