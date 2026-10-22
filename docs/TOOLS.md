# Analysis tooling

Everything needed to reverse-engineer the two ROMs headlessly.

## ROMs
Kept in `roms/` (git-ignored, never committed):
- `roms/Earthworm Jim (USA).md` - Earthworm Jim, USA, 3 MB, CRC32 `DF3ACF59` (`mdt.EWJ1`, `m68dis` key `ewj1`)
- `roms/Earthworm Jim 2 (USA).md` - Earthworm Jim 2, USA, 3 MB, CRC32 `D57F8BA7` (`mdt.EWJ2`, key `ewj2`)
- The Europe dumps sit alongside for reference; they are not supported targets.

## `tools/mdt.py` — scriptable emulator (wraps `tools/bin/mdtool.dll`)
Our own Mega Drive core (68000/Z80/VDP exact enough to run both games; sound chips are stubbed in this
DLL build, which does not matter for analysis). NTSC, 60 fps, 3-button pad on port 1.

```python
import sys; sys.path.insert(0, r'<repo>\tools')
from mdt import MD, COI, QS
md = MD(COI)
md.run(600)                 # run frames
md.press('START', 3, 60)    # hold START 3 frames, then release and run 60 frames
md.hold('RIGHT+C')          # hold buttons until changed: UP DOWN LEFT RIGHT A B C START joined by '+'
md.hold2('LEFT')            # player 2 pad (port 2), same syntax
md.run(120); md.hold(0)
md.png(r'...\shot.png', 2)  # screenshot (scale 2). View PNGs with the Read tool.
md.ram16(0xFFF400); md.set16(0xFFF400, 3)   # RAM access (68000 addresses)
md.ram / md.vram / md.cram / md.vsram / md.regs / md.zram / md.rom   # raw arrays (live, writable)
md.cpu()                    # 68000 registers dict (d0..a7, pc, sr)
md.z80()                    # Z80 registers + bank/reset/busreq
s = md.save(); md.load(s)   # save states (bytes) - only valid with this DLL build
md.set_ext(40, sprite_limit=False)  # render 40 extra px each side (widescreen preview: 400x224)
md.patch16(addr, value)     # patch the loaded ROM copy (word)
md.coverage(True); ...; md.covered(lo, hi)   # executed instruction addresses with counts
md.bp(addr); md.run(n); md.bp_hits()         # log registers every time PC hits addr
md.watch(lo, hi); md.run(n); md.watch_hits() # log 68000 writes to RAM range: frame, pc, addr, size, value
md.unwatch(); md.bp_clear()
```
Frame buffer colour-code note: `md.pixels()` returns (w, h, list of 0xAARRGGBB).

## `tools/m68dis.py` — disassembler (capstone)
```
python tools/m68dis.py ewj1 0x200 40        # 40 instructions at $200 with bytes
python tools/m68dis.py qs xref 0xFFEE00     # instructions referencing an address (abs.w/abs.l)
python tools/m68dis.py ewj1 calls 0xCF92     # JSR/JMP/BSR/Bcc targeting an address
```
From Python: `from m68dis import dis, insns, xrefs, calls, u16, u32, rom`.
Capstone prints absolute-short RAM addresses as e.g. `$f400.w` meaning `$FFF400`.

## Save states
`tools/states/*.state` (git-ignored) hold handy starting points you make yourself, e.g. `ewj1_level1.state`.

## Known facts
- Both games run in H40 (320x224), NTSC.
- Both use the VDP window plane for the bottom status bar (reg 18 = $98 → window from line 192).
- COI gameplay planes are 64x64 cells; QS gameplay planes are 64x32 cells.
- World of Illusion (1 MB ROM, supports 2 players simultaneously): gameplay planes 64x32 cells, per-line
  hscroll, no window plane; the HUD (character icon, hearts, TRIES) is drawn with sprites at the top.
  Its VBlank handler is at $D74 (dispatch on $FFFFD2); HINT vector points to RAM ($FFAF80).

## Running the tools on Linux
`src/tools/mdtool.cpp` also builds as a shared library for analysis away from Windows:

    g++ -std=c++17 -O2 -shared -fPIC -Isrc src/core/*.cpp src/game/common.cpp src/game/ewj1.cpp src/game/ewj2.cpp src/tools/mdtool.cpp -o libmdtool.so
    MDTOOL_DLL=./libmdtool.so python3 your_script.py

`tests/cheat_test.cpp` (see its header) checks the cheats against the real ROMs without any Python.
