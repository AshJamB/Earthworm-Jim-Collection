// Headless check of the game modules' cheats against the real ROMs (which are not in the repo).
// Build (Linux/MSVC-free):  g++ -std=c++17 -Isrc tests/cheat_test.cpp src/core/*.cpp src/game/common.cpp src/game/ewj1.cpp src/game/ewj2.cpp -o cheat_test
// Run:  ./cheat_test "roms/Earthworm Jim (USA).md" "roms/Earthworm Jim 2 (USA).md"
// Boots each game into the first level (a few START presses), then plays 1,200 frames firing the gun and checks
// that, with the cheats on, health/lives/ammo stay full, and with them off, the ammo goes down. Also checks level select.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
#include "../src/core/md.h"
#include "../src/game/game.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void press(MegaDrive& md, uint16_t btn, int hold, int after, GameModule* m, const GameConfig& cfg) {
    md.set_pad(0, btn);
    for (int i = 0; i < hold; i++) { m->pre_frame(md, cfg); md.run_frame(); }
    md.set_pad(0, 0);
    for (int i = 0; i < after; i++) { m->pre_frame(md, cfg); md.run_frame(); }
}

static void run(MegaDrive& md, int n, GameModule* m, const GameConfig& cfg) {
    for (int i = 0; i < n; i++) { m->pre_frame(md, cfg); md.run_frame(); }
}

// Level select: each stage must load something different (checked by hashing video memory), and the game's own
// level byte must read back as the stage that was asked for.
static void test_levels(MegaDrive& md, GameModule* m, uint32_t level_addr) {
    const std::vector<LevelEntry> lv = m->levels();
    CHECK(!lv.empty());
    GameConfig cfg;
    uint64_t prev = 0;
    int distinct = 0;
    for (int i : {0, 1, 4, 7, (int)lv.size() - 1}) {
        if (i >= (int)lv.size()) continue;
        CHECK(m->start_level(md, cfg, i, 0));
        uint64_t h = 1469598103934665603ull;
        for (uint8_t b : md.vdp.vram) h = (h ^ b) * 1099511628211ull;
        if (h != prev) distinct++;
        prev = h;
        CHECK(md.ram8(level_addr) == (uint8_t)i);
    }
    printf("%s level select: %d stages listed, %d of 5 samples looked different\n", m->id(), (int)lv.size(), distinct);
    CHECK(distinct >= 4);
    CHECK(!m->start_level(md, cfg, (int)lv.size(), 0));   // out of range is refused
}

static void test_game(const char* path, GameModule* m, uint32_t ammo, uint32_t health, uint32_t lives, uint32_t level_addr, int starts, bool unpause) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { printf("skip %s (ROM not found)\n", path); return; }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    for (int cheats = 0; cheats < 2; cheats++) {
        MegaDrive md;
        CHECK(md.load_rom(rom));
        md.power_on();
        GameConfig cfg;
        m->configure(md, cfg);
        GameConfig none = cfg;   // no cheats while booting
        run(md, 400, m, none);
        for (int i = 0; i < starts; i++) press(md, 0x80 /* START */, 4, 150, m, none);
        run(md, 200, m, none);
        if (unpause) press(md, 0x80, 4, 120, m, none);   // the last START press paused the level
        run(md, 200, m, none);
        if (cheats) { cfg.cheats["inf_health"] = true; cfg.cheats["inf_lives"] = true; cfg.cheats["inf_ammo"] = true; }
        for (int i = 0; i < 40; i++) press(md, 0x40 /* A: fire */, 6, 24, m, cfg);   // ~1,200 frames
        auto text = [&](uint32_t a, int n) { std::string s; for (int i = 0; i < n; i++) s += (char)md.ram8(a + i); return s; };
        printf("%s cheats=%d: ammo=%s health=%s lives=%c\n", m->id(), cheats, text(ammo, 4).c_str(), text(health, 4).c_str(),
               (char)md.ram8(lives));
        if (cheats) {
            CHECK(text(ammo, 4) == "1000");
            CHECK(text(health, 4) == "0100");
            CHECK(md.ram8(lives) == '9');
        } else {
            CHECK(text(ammo, 4) != "1000");   // firing used ammo
            CHECK(md.ram8(lives) == '3');
        }
        if (cheats == 0) { MegaDrive lv; CHECK(lv.load_rom(rom)); test_levels(lv, m, level_addr); }
    }
}

int main(int argc, char** argv) {
    std::unique_ptr<GameModule> m1(create_ewj1_module()), m2(create_ewj2_module());
    if (argc > 1) test_game(argv[1], m1.get(), 0xFFFBF4, 0xFFFBFC, 0xFFA6C8, 0xFFA692, 3, false);
    if (argc > 2) test_game(argv[2], m2.get(), 0xFFFADE, 0xFFFAFC, 0xFFA1EA, 0xFFA1A6, 10, true);
    if (failures == 0) printf("cheat_test: all checks passed\n");
    return failures ? 1 : 0;
}
