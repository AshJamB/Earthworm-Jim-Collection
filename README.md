# Earthworm Jim Collection

A Windows and Android port of the Sega Mega Drive / Genesis games **Earthworm Jim** (1994) and
**Earthworm Jim 2** (1995), with a launcher, touch controls, gamepad support, save states, rewind and fast
forward.

It is derived from the MIT-licensed [Mickey and Donald Collection](https://github.com/Darhkfox/MickeyAndDonaldCollection)
by Darhkfox (see `LICENSE`), keeping its from-scratch Mega Drive core and frontend and replacing the game modules.

The games run from **your own copies of the original ROMs**. This repository contains no game code or assets.

## Status

Early scaffold. Both games are recognised by CRC32 and run unmodified. Not yet implemented:
widescreen, cheats, level select, music player track lists, achievements (see `src/game/ewj1.cpp` and `ewj2.cpp`).

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

## Licence and credits

MIT (`LICENSE`). Core and frontend by Darhkfox. *Earthworm Jim* is a trademark of its respective owners; this is an
unofficial fan project, not affiliated with or endorsed by them.
