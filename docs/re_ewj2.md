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

## Still to find (remaining)
Music player (sound driver), widescreen.

## Level select (src/game/ewj2.cpp)
The level number is the byte at **`$FFA1A6`**, taken by the loader when START is pressed on the title screen
(the fourth START: SEGA logo, legal text, Shiny logo, then "Press Start"). `start_level` plays the start-up from
power on, writes the byte before that press and runs 500 frames afterwards.

Found the same way as in the first game (set each byte of `$FFA100-$FFA300` to 1 before the press and compare
video memory). Values 0-16 load stages and 17 and above a black screen. The names are from the game's hidden debug
menu ("SUPER CHEAT SCREEN"), whose START ON LEVEL list is stored as 16-character strings at ROM `$1C0A1D` in the same
order as the byte's values:

| Value | Stage | Value | Stage |
| ----- | ----- | ----- | ----- |
| 0 | 1 Tangerines | 9 | 6 Villi People |
| 1 | 1B Granny Chair | 10 | 7 Lorenzen's Soil |
| 2-4 | 2A-2C Puppy Love 1-3 | 11 | 8 Level Ate |
| 5 | 3 The Flyin' King | 12 | 8A Flamin' Yawn |
| 6 | 4 Udderly Abducted | 13 | 8B Forked |
| 7 | 5A Inflated Head | 14 | 9 ISO 9000 |
| 8 | 5C Hammer Head | 15 | 9B Door Chase |
| 16 | 10 See Jim Run | | |


## Widescreen (map streaming and enemy placement)

Same engine as the first game, at a fixed offset of -$504 in work RAM (camera X $FFA172, Y $FFA174, snapped camera
$FFA182/$FFA184, map width in px $FFA16C, level number $FFA1A6). Two scrolling layers each have a copy of the column and
row routines: plane A at $2530xx-$2533xx and plane B at $2534xx-$2537xx. Enemies are placed by $256510/$25651A (columns,
x offset $1150 = $1000 + 336) and $2565B4/$2565BE (rows). `widescreen_patches()` in `ewj2.cpp` lists the twelve
immediates changed (22 -> 24 columns, 23 -> 25 for rows, offset $1150 -> $1170).

The cells left of the screen are redrawn each frame like in the first game (`refresh_left_columns`): plane A uses
$FFA182/$FFA184, the table at $FFB262 and row offsets at $FFB4E6; plane B (only while $FFFBB2 is set) uses $FFA18E/$FFA190,
$FFB362 and $FFB676; both use the stride at $FFA166, the metatile table pointer at $FFA1D0 and give tile 0 at or past the
limit at $FF9416. Skipped while $FFFF15 is set.

The same sprite cull (`cmpi.w #$1BF,d3`) is at $255CF0, $255D52, $255DB4, $255E1C, $255E7C, $255EE4, $255F4C and $255FBA (immediates
patched to $1EF). The camera is up to 18 px either side of the snapped position ($FFA182).

Left-column spawn: as in the first game: the pointer store at $252FB6 goes through a stub at $2FEE00, offset $0FF0 at $256512
becomes $0FC0. Neither is done for the rows drawn when the camera moves up or down.

## HUD anchoring (widescreen)

The status bar sprites go through the fixed-position branch ($6(a2) bit 3) of the four sprite-builder variants at $255CEA,
$255D4C, $255DAE and $255E16. With HUD anchoring they call a stub at $2FEE40 that moves a piece of an object near the left
edge (x < $60) `ext` px left and one near the right edge (x >= $E0) `ext` px right, and sets bit 15 of its y (the VDP's HUD
flag). The left-cull compare at $255E78 / $255EE0 / $255F48 / $255FB6 is widened from $60 to $30.
The HUD text and counters (lives, health, ammo) are written into the sprite buffer at $FF964E (from $255762) before the
objects; the join $255AF2 (`clr.b $FFFBD7`) calls a stub at $2FEE80 that moves everything in the buffer so far `ext` px left
and flags it.

## Sound driver (music player / sound test)

Z80 driver (loaded from ROM into Z80 RAM in the first frames after power-on; the 68000 sends its sample-table pointers with
command $0B). The 68000 appends commands to a 64-byte ring at Z80 $1B40 with its write index at Z80 $36, each as `$FF`,
command, argument (routine `$2CBC82` in EWJ1, `$2D8280` in EWJ2 appends one byte). `$16` stops all sound, `$10 id` starts a
tune or effect. The port parks the 68000 after the driver is up and writes the ring itself. Ids 0-127 were each played for up to 10 s:
looping ids are the level tunes, a few run 4-9 s (jingles), the rest are effects.

## Widescreen: vertical spawn scan

The row spawn scan ($2565B4/$2565BE, body $2565C6) now starts up to 3 columns further left (stub at $2FEEC0 replaces the load of
the start column at $2565EA) and covers 27 columns, like EWJ1's.
