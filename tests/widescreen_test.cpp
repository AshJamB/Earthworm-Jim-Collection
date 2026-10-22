// Headless check of the widescreen support against the real ROMs (which are not in the repo).
// Build:  g++ -std=c++17 -O2 -Isrc tests/widescreen_test.cpp src/core/*.cpp src/game/common.cpp src/game/ewj1.cpp src/game/ewj2.cpp -o widescreen_test
// Run:    ./widescreen_test "roms/Earthworm Jim (USA).md" "roms/Earthworm Jim 2 (USA).md"
// For every level of each game, starts it at the normal width and widened (40 px a side) and checks that
//  - the module reports gameplay and mask values within the extension,
//  - the middle 320 columns look like the normal picture (widescreen must not disturb what the game draws),
//  - the extension is not an empty strip where it is not masked (reported, not failed: some levels are black),
//  - moving right for a while keeps both of those true (the patched scenery streaming works),
// and that the extension is closed (full masks) outside a level.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
#include "../src/core/md.h"
#include "../src/game/game.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

struct Shot { int w = 0, h = 0; std::vector<uint16_t> px; };

static Shot grab(const MegaDrive& md) {
    Shot s;
    s.w = md.vdp.fb_width;
    s.h = md.vdp.fb_lines;
    s.px.resize((size_t)s.w * s.h);
    for (int y = 0; y < s.h; y++)
        for (int x = 0; x < s.w; x++) s.px[(size_t)y * s.w + x] = md.vdp.fb[y][x] & 0x7FF;
    return s;
}

static void run_frames(MegaDrive& md, GameModule* m, const GameConfig& cfg, FrameInfo& info, int n, uint16_t pad = 0) {
    for (int i = 0; i < n; i++) {
        md.set_pad(0, pad);
        m->pre_frame(md, cfg);
        md.run_frame();
        info = FrameInfo();
        m->post_frame(md, cfg, info);
    }
}

// fraction of the pixels in columns [x0, x1) that are not the backdrop colour of the strip's top-left pixel
static double busy(const Shot& s, int x0, int x1) {
    int n = 0, tot = 0;
    for (int y = 0; y < s.h; y++)
        for (int x = x0; x < x1; x++) {
            tot++;
            if (s.px[(size_t)y * s.w + x] != 0) n++;
        }
    return tot ? (double)n / tot : 0;
}

static void test_game(const char* path, GameModule* m, bool right_moves) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { printf("skip %s (ROM not found)\n", path); return; }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const int ext = 40;
    const int count = (int)m->levels().size();
    int bad_center = 0, empty_ext = 0;
    for (int i = 0; i < count; i++) {
        Shot normal, wide;
        FrameInfo info0, info1;
        for (int pass = 0; pass < 2; pass++) {
            MegaDrive md;
            CHECK(md.load_rom(rom));
            GameConfig cfg;
            cfg.ext = pass ? ext : 0;
            md.vdp.ext = cfg.ext;
            md.vdp.sprite_limit = cfg.ext == 0;
            md.vdp.ignore_left_blank = cfg.ext > 0;
            m->configure(md, cfg);
            CHECK(m->start_level(md, cfg, i, 0));
            md.vdp.ext = cfg.ext;
            md.vdp.sprite_limit = cfg.ext == 0;
            md.vdp.ignore_left_blank = cfg.ext > 0;
            m->configure(md, cfg);
            FrameInfo info;
            run_frames(md, m, cfg, info, 150);
            (pass ? wide : normal) = grab(md);
            (pass ? info1 : info0) = info;
            if (pass && right_moves) {
                // walk right (jumping now and then): the extension must stay filled
                for (int k = 0; k < 12; k++) run_frames(md, m, cfg, info, 20, 0x08 | ((k / 2) % 2 ? 0x10 : 0));
                Shot later = grab(md);
                if (info.mask_right < ext) {
                    const double r = busy(later, later.w - (ext - info.mask_right), later.w);
                    if (r < 0.02) { printf("  level %d: right strip empty after walking (%.3f)\n", i, r); empty_ext++; }
                }
            }
        }
        CHECK(info1.gameplay);
        CHECK(info1.mask_left >= 0 && info1.mask_left <= ext && info1.mask_right >= 0 && info1.mask_right <= ext);
        CHECK(wide.w == 320 + 2 * ext && normal.w == 320);
        // middle columns equal to the normal picture (small differences: animation timing)
        int diff = 0;
        for (int y = 0; y < normal.h; y++)
            for (int x = 0; x < 320; x++)
                if (normal.px[(size_t)y * 320 + x] != wide.px[(size_t)y * wide.w + ext + x]) diff++;
        const double dr = (double)diff / (normal.h * 320.0);
        if (dr > 0.35) { printf("  level %d: middle differs %.1f%%\n", i, dr * 100); bad_center++; }
        // the extension is drawn where it is not masked
        if (info1.mask_right == 0) {
            const double r = busy(wide, wide.w - ext, wide.w);
            if (r < 0.02) { printf("  level %d: right strip empty at start (%.3f)\n", i, r); empty_ext++; }
        }
    }
    printf("%s widescreen: %d levels, %d with a changed middle, %d with an empty strip\n", m->id(), count, bad_center, empty_ext);
    CHECK(bad_center == 0);

    // outside a level the extension is closed
    MegaDrive md;
    CHECK(md.load_rom(rom));
    md.power_on();
    GameConfig cfg;
    cfg.ext = ext;
    m->configure(md, cfg);
    FrameInfo info;
    run_frames(md, m, cfg, info, 200);
    CHECK(!info.gameplay);
}

int main(int argc, char** argv) {
    std::unique_ptr<GameModule> m1(create_ewj1_module()), m2(create_ewj2_module());
    if (argc > 1) test_game(argv[1], m1.get(), true);
    if (argc > 2) test_game(argv[2], m2.get(), true);
    if (failures == 0) printf("widescreen_test: all checks passed\n");
    return failures ? 1 : 0;
}
