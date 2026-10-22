# Earthworm Jim Collection

A Windows and Android port of the Sega Mega Drive / Genesis games **Earthworm Jim** (1994) and
**Earthworm Jim 2** (1995), with a launcher, touch controls, gamepad support, save states, rewind and fast
forward.

It is derived from the MIT-licensed [Mickey and Donald Collection](https://github.com/Darhkfox/MickeyAndDonaldCollection)
by Darhkfox (see `LICENSE`), keeping its from-scratch Mega Drive core and frontend and replacing the game modules.

The games run from **your own copies of the original ROMs**. This repository contains no game code or assets.

## Status

Early development (v0.5.0). Both games are recognised by CRC32 and run. Cheats (infinite health, lives and ammo) and level
select (29 stages in Earthworm Jim, 17 in Earthworm Jim 2) work.
Widescreen (16:9, on by default) shows 40 extra pixels each side. The games' map streaming and enemy placement are
patched to work 2 columns further out, so scenery and enemies are in place before they enter the wider view; the
background is mirrored where a game leaves it empty; the extension is black beyond the edges of a level and in a few
single-screen arenas. The status bar anchors to the left edge. The Music Player and Sound Test play the games' own tunes
and effects through their Z80 sound driver (tunes are named by the level they play in where known). Not yet implemented:
RetroAchievements, tune names for every track (see
`src/game/ewj1.cpp` and `ewj2.cpp`, and `docs/`).

## ROMs

Target dumps (USA, 3 MB):

| Game             | CRC32      |
| ---------------- | ---------- |
| Earthworm Jim    | `DF3ACF59` |
| Earthworm Jim 2  | `D57F8BA7` |

ROMs are found by CRC32, in the exe folder or a `roms` folder next to it (Android: use **Add ROMs**). The `roms/`
folder in this repo is git-ignored. Other regions (Europe) are recognised as "another version" but not supported.

## Building

See `build.bat` (Windows, Visual Studio 2022 + CMake) and `android/` (SDK, NDK 28.2, `android\fetch_sdl.ps1`).
The code has not been compiled since the rebrand: expect small fixes on the first build.

## Updates

On Windows the app checks this repository's latest GitHub Release at start-up (switch it off under Options) and
can update itself from **Credits > Check for Updates**. This needs the repository's releases to be publicly readable;
while they are not, the start-up check fails silently. Releases are built and published automatically by
`.github/workflows/release.yml` when the program changes on `main` (see the comments in that file for how the
version number is chosen). On Android, Check for Updates opens the Releases page.

## Licence and credits

MIT (`LICENSE`). Core and frontend by Darhkfox. *Earthworm Jim* is a trademark of its respective owners; this is an
unofficial fan project, not affiliated with or endorsed by them.
