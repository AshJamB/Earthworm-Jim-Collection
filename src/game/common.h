// Helpers shared by the game modules: verified ROM patching and window-plane HUD remapping.
#pragma once
#include <cstdint>
#include <vector>
#include <initializer_list>
#include <functional>

class MegaDrive;
class VDP;

struct RomPatch {
    uint32_t addr;
    std::vector<uint16_t> orig;   // expected original words (patch skipped if they don't match)
    std::vector<uint16_t> repl;   // replacement words
};

// Restores the pristine ROM, then applies the patches and fixes the header checksum word
// ($18E) so the games' boot-time checksum test still passes. Returns the number applied.
int apply_patches(MegaDrive& md, const std::vector<RomPatch>& patches);

// Whether the ROM can go from what it holds now to what `patches` give without pulling code from under the
// 68000: no word that would change may be at the PC or behind a return address on the stack. A width or
// cheat change made while the game runs patched code waits (Session) until this holds.
bool patches_safe(const MegaDrive& md, const std::vector<RomPatch>& patches);

// Scripted-input helpers for level select / jukebox boots (run headless, as fast as possible).
bool run_until(MegaDrive& md, const std::function<bool()>& cond, int max_frames);
// Taps `button` (MdButton bits) until cond() holds: waits for idle() (game saw the pad released),
// holds up to hold_frames, releases, retries. Returns with the pad released.
bool press_until(MegaDrive& md, uint16_t button, const std::function<bool()>& cond,
                 const std::function<bool()>& idle, int hold_frames = 3, int tries = 20);
// Parks the 68000 for good (interrupts masked) and releases the Z80 bus, leaving the Z80 sound
// driver running on its own - used by the music player.
void freeze_68k(MegaDrive& md);
// Runs the 68000 subroutine at `entry` until it returns, on a parked CPU (freeze_68k), with interrupts masked; the
// CPU is parked again afterwards. Only the 68000 runs. Returns false if it hasn't returned after `max_cycles`.
bool call_68k(MegaDrive& md, uint32_t entry, int max_cycles = 2000000);

// Widescreen background fill: a game that only draws the 40 cells it shows leaves the cells just outside the screen
// empty (black) or holding leftovers. This fills them with a mirror image of the cells next to them (flipped tiles),
// so the extension carries on the scenery. It works on a plane that scrolls as a whole (horizontal scroll mode 0, 64
// cells wide). A plane that has not scrolled at all since the level began (reset()) is a single picture (a sky): the cells beside it are all
// replaced, and put back when the plane moves again. A moving plane only has its empty cells beside it filled.
class EdgeFill {
public:
    void run(MegaDrive& md, int ext, bool plane_b);
    void reset() { saved_.clear(); still_ = 0; last_scroll_ = -1; moved_ = false; }
private:
    struct Saved { uint32_t addr; uint16_t orig, put; };
    std::vector<Saved> saved_;
    int still_ = 0, last_scroll_ = -1;
    bool moved_ = false;   // the plane has scrolled since reset(): it is a scrolling background, not a single picture
};

// The games draw their scrolling level maps into a 64-cell ring in VRAM one 16-px metatile column (or row) at a time,
// just ahead of the camera, so the cells beside the 4:3 view go stale: after walking right and back, or moving up and
// down, the strip left of the screen holds other parts of the map. This redraws that strip every frame, with the
// game's own drawing rules (which RAM tables a layer uses is in the description).
struct StreamLayer {
    uint32_t snap_x, snap_y;     // RAM words: the camera snapped to 16 px (the position the game draws from)
    uint32_t vram_tab;           // RAM table of VDP address commands, one long per nametable cell row
    uint32_t rowptr_tab;         // RAM table of map row pointers (per 16-px row)
    bool     rowptr_is_long;     // long pointer (else a word offset into work RAM)
    int      rowptr_shift;       // map row number -> table offset: (y & 0xFFF0) >> shift
    uint32_t stride;             // RAM word: bytes from one map row to the next
    uint32_t tiles;              // RAM long: pointer to the metatile table (4 words each)
    uint32_t limit;              // RAM long: map reads at or past this address give tile 0 (0 = none)
};
// Redraws `cols` metatile columns left of the snapped camera (pixel columns snap_x - 16 ... snap_x - 16 * cols).
// With `verify`, writes nothing and returns how many cells differ from what is in VRAM instead (for tests).
int refresh_left_columns(MegaDrive& md, const StreamLayer& layer, int cols, bool verify = false, int first = 1, int dir = -1);
// refresh_left_columns, but only after checking the same drawing rules against the columns just inside the view, which
// the game drew itself: if they differ (a map edge, a level that draws its map another way, a frame in which the game is
// part-way through scrolling) the strip is left as it is. Returns whether it was redrawn.
bool refresh_left_strip(MegaDrive& md, const StreamLayer& layer, int cols);
// The same for both strips beside the screen: `ext` px left of the camera, and the columns from the right edge of the
// 4:3 view out to `ext` px beyond it (the ring holds old map there after the camera has walked left).
bool refresh_edge_strips(MegaDrive& md, const StreamLayer& layer, int ext);

// HUD strip remapping for widescreen: the window-plane status bar is split into segments
// that anchor to the left edge, the centre or the right edge of the widened screen; gaps are
// filled with a repeated "filler" column span of the bar.
struct HudSegment {
    int src_x0, src_x1;   // source pixels in the original 320-px bar
    int anchor;           // -1 = left edge, 0 = centre (unchanged), 1 = right edge
};

struct HudLayout {
    int y0 = 0, y1 = 0;                 // lines covered by the bar
    std::vector<HudSegment> segments;
    int filler_x0 = 0, filler_x1 = 8;   // source span repeated to fill gaps (multiple of 8 recommended)
    bool clear_plane_b = true;          // hide plane B behind the bar in the extension
    // output x (0 .. 320 + 2*ext) -> source x (0..319), built by build()
    std::vector<int16_t> map;
    int built_ext = -1;
    void build(int ext, int width = 320);
};

// Rewrites a line's plane-A buffer from the window plane using the layout's map.
void hud_remap_line(VDP& vdp, int line, uint8_t* plane_a, uint8_t* plane_b, HudLayout& layout);

// Per-pixel variant: map[x] (x = 0 .. width+2*ext-1) = source pixel of the original bar, or -1
// for transparent. Plane B is cleared in the extension (the bar hides it in the original view).
void hud_remap_window_line(VDP& vdp, int line, uint8_t* plane_a, uint8_t* plane_b, const std::vector<int16_t>& map);
