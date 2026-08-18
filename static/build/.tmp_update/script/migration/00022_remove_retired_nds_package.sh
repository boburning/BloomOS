#!/bin/sh

# BloomOS does not redistribute the inherited proprietary DraStic package.
# Remove only its emulator integration from upgraded cards; Nintendo DS ROMs,
# saves, and other user content live outside this directory and are preserved.
old_dir="/mnt/SDCARD/Emu/NDS"

if [ -d "$old_dir" ] && [ ! -L "$old_dir" ]; then
    rm -rf "$old_dir"
fi
