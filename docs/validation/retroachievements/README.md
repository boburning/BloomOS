# RetroAchievements physical certification evidence

These records certify exact Bloom core binaries on physical Miyoo hardware.
They never contain ROMs, BIOS files, credentials, download locations, or raw
content hashes. Operator-provided fixtures are recorded by RetroAchievements
Game ID only.

A core remains `untested` or `best_effort` until every required category has a
real result. Seeing the achievement list is not certification. For gpSP, test:

- identification and wrong-hash rejection;
- real softcore unlocks across representative WRAM, IWRAM, and save-RAM use;
- Rich Presence and a leaderboard where available;
- Hardcore start, prohibited-feature behavior, reset, and state behavior;
- RTC, demanding, and save-heavy games;
- cold launch, normal exit, GameSwitcher transition, save flush, and reconnect;
- frame pacing, audio, memory, launch time, and regression against mGBA.

Run the matrix on each applicable Mini-family device. The original Mini has no
network transport, so its direct-online cases are recorded `not_applicable`
rather than inferred from Plus results. Public records may identify the
operator but should not identify private ROM filenames or paths.

Copy `gpsp-template.json` for each run, fill only observed results, and validate
it before changing `build/ra-core-policy.json`. A binary SHA mismatch requires
a new certification run.
