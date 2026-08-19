#!/usr/bin/env bats

@test "SNES package defaults to current Snes9x" {
    static_launcher='/workspace/static/packages/Emu/Nintendo - SNES (Beetle Supafaust)/Emu/SFC/launch.sh'
    grep -F 'snes9x_libretro.so' "$static_launcher"
    run grep -F 'mednafen_supafaust_libretro.so' "$static_launcher"
    [ "$status" -ne 0 ]
}

@test "SG-1000 package defaults to Genesis Plus GX" {
    static_launcher='/workspace/static/packages/Emu/Sega - SG-1000 (Gearsystem)/Emu/SEGASGONE/launch.sh'
    grep -F 'genesis_plus_gx_libretro.so' "$static_launcher"
    run grep -F 'gearsystem_libretro.so' "$static_launcher"
    [ "$status" -ne 0 ]
}
