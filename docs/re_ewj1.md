# Earthworm Jim (USA) - reverse engineering notes

ROM: `Earthworm Jim (USA)`, 3 MB, CRC32 `DF3ACF59`, header product `GM T-132036-00`.
Found with the headless core (`tools/mdt.py`, built as a shared library; see `docs/TOOLS.md`).

## Reaching the first level
Title -> START three times (each ~150 frames apart) starts New Junk City. START in a level is **pause**, so a
fourth press stops the game (about 2 RAM bytes change per 30 frames while paused, ~140 when running).

## Counters (68000 RAM)
The game keeps its HUD counters as **ASCII decimal text** and counts them up and down on the characters themselves
(a watch on the ammo text shows the shot routine at `$24B2E8` decrementing the last digit).

| Counter | Address | Format | Start value |
| ------- | ------- | ------ | ----------- |
| Plasma ammo | `$FFFBF4` | 4 ASCII chars | `1000` |
| Suit energy (health) | `$FFFBFC` | 4 ASCII chars | `0100` |
| Lives | `$FFA6C8` | 1 ASCII char | `3` |

- Firing with A removes about one per frame held (1000 -> 994 after six frames).
- Writing `0000` to the health text kills Jim: he respawns with `0100`, and the lives character drops from `3` to `2`.
- Writing `9` to the lives character shows `x9` in the HUD.

## Cheats (src/game/ewj1.cpp, `pre_frame`)
Each cheat rewrites its counter every frame, only while the health text is four ASCII digits (so title and menu
screens, which reuse the memory, are untouched). Infinite health leaves a health of `0000` alone, so a death that
has already started still plays. Pits and scripted deaths still cost a life.

Tested headlessly by `tests/cheat_test.cpp` against the real ROM.

## Still to find
Level index and the transition code (level select), the sound driver request slots (music player), and the
sprite/despawn/camera logic (widescreen).
