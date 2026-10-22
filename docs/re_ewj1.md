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

## Still to find (remaining)
The sound driver request slots (music player), and the sprite/despawn/camera logic (widescreen).

## Level select (src/game/ewj1.cpp)
The level number is the byte at **`$FFA692`**. The game's loader takes it when START is pressed on the title screen, so
writing a stage number just before that press starts that stage; `start_level` plays the game's own start-up from power
on (400 frames, START, 150, START, 60), writes the byte, then presses START and runs 600 frames.

Found by saving a state before the final START, then setting each byte of `$FFA500-$FFA900` to 1 in turn and
comparing video memory 300 frames later; only `$FFA692` produced a different level.

Values 0-28 load stages; 29 and above load a black screen. The names are from the game's own hidden debug menu
("David Perry's Private Cheat Mode": pause, then Left+A, B, B, A, Right+A, B, B, A), whose START ON LEVEL list is
stored as 16-character strings at ROM `$1B6722`, in the same order as the byte's values:

| Value | Stage | Value | Stage |
| ----- | ----- | ----- | ----- |
| 0 | 1 New Junk City | 15 | 8B Pod Races |
| 1 | 2A Hell or Heck? | 16-22 | 9A-9G Asteroids 1-7 |
| 2 | 2B Snowman Boss | 23-27 | 10A-10E Darkness 1-5 |
| 3 | 2C Evil Boss | 28 | 9P Psy-Crow! |
| 4-6 | 3A-3C Bungee 1-3 | 7 | 4 Peter Puppy |
| 8 | 5 Slug for a Butt | 9 | 5B Helicopter |
| 10 | 6A Prof's Lab | 11 | 6C Chicken Fly |
| 12 | 6D Naked Worm | 13 | 7 Intestines |
| 14 | 8A Sea Tunnels | | |

The same menu also offers CHEAT MODE (invincibility), FREEZEABILITY and MAP VIEW MODE.


## Widescreen (map streaming and enemy placement)

The level map is drawn into plane A one 16-px column at a time as the camera moves ($FFA676 = camera X, $FFA678 = Y;
$FFA686/$FFA688 = the camera snapped to 16 px, stepped by the routines at $248024 (left/right) and $248228/$248330
(down/up)). New columns come in 22 map columns (352 px) right of the camera; the enemies of that column are placed by the
scan at $24AF0E (x offset $150 = 336, the column's 352 minus a 16 px bias) and, for rows, $24AF86/$24AF90 (23 columns).
The object loop at $24A766 despawns an object when it is more than 120 px left of the camera or 440 px right of it, so
a wider view does not lose them. `widescreen_patches()` in `ewj1.cpp` changes 22 -> 24 columns (map offset $2C -> $30,
VRAM column offset $58 -> $60, map end test $16 -> $18), the enemy offset $150 -> $170 and the row widths $16 -> $18.
The level's map size is at $FFA670 (px) and $FFA674; the level number is $FFA692.
