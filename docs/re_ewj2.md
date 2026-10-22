# Earthworm Jim 2 (USA) - reverse engineering notes

ROM: `Earthworm Jim 2 (USA)`, 3 MB, CRC32 `D57F8BA7`, header product `GM T-132056-00`.

## Reaching the first level
Title -> START ten times (150 frames apart) reaches Andy Asteroids' first level but leaves it **paused**; one more
START press unpauses. About 70 RAM bytes change per 30 frames when it runs, 1-3 when paused.

## Counters (68000 RAM)
Same scheme as the first game: ASCII decimal text counted on directly.

| Counter | Address | Format | Start value |
| ------- | ------- | ------ | ----------- |
| Plasma ammo | `$FFFADE` | 4 ASCII chars | `1000` |
| Suit energy (health) | `$FFFAFC` | 4 ASCII chars | `0100` |
| Lives | `$FFA1EA` | 1 ASCII char | `3` |

- Firing with A: `1000` -> `0994` after six frames held.
- Writing `0000` to the health text kills Jim and the lives character drops from `3` to `2` on respawn.
- Writing `9` to `$FFA1EA` shows `x9`.
- Not the lives: `$FFA200` also reads 3 -> 2 around a death but is a per-frame countdown timer (written by `$24EB7A`).
  Candidates must be checked by writing a new value, not only by watching them change.

## Cheats (src/game/ewj2.cpp)
Identical to the first game, with these addresses. Tested by `tests/cheat_test.cpp`.

## Still to find
Level select, music player (sound driver), widescreen.
