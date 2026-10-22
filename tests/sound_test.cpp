// Headless check of the music player / sound test support against the real ROMs (which are not in the repo).
// Build:  g++ -std=c++17 -O2 -Isrc tests/sound_test.cpp src/core/*.cpp src/game/common.cpp src/game/ewj1.cpp src/game/ewj2.cpp -o sound_test
// Run:    ./sound_test "roms/Earthworm Jim (USA).md" "roms/Earthworm Jim 2 (USA).md"
// On a machine readied by jukebox_boot: silence at first; every tune starts to sound; a tune keeps sounding for many
// seconds (they loop) and a jingle ends; stop_sound makes it quiet again; an effect is short.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>
#include "../src/core/md.h"
#include "../src/game/game.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static int range_of(const MegaDrive& md) {
    int32_t mn = 1 << 30, mx = -(1 << 30);
    for (int k = 0; k < md.audio_frames * 2; k++) { mn = std::min(mn, md.audio[k]); mx = std::max(mx, md.audio[k]); }
    return md.audio_frames ? mx - mn : 0;
}
// frames (of n) with sound
static int loud_frames(MegaDrive& md, int n) { int c = 0; for (int i = 0; i < n; i++) { md.run_frame(); c += range_of(md) > 60; } return c; }

static void test_game(const char* path, GameModule* m) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { printf("skip %s (ROM not found)\n", path); return; }
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const std::vector<SoundEntry> list = m->sounds();
    CHECK(list.size() > 20);
    int tunes = 0, effects = 0;
    for (const SoundEntry& e : list) (e.music ? tunes : effects)++;
    printf("%s: %d tunes, %d effects\n", m->id(), tunes, effects);
    CHECK(tunes >= 10 && effects >= 50);

    MegaDrive md;
    CHECK(md.load_rom(rom));
    m->jukebox_boot(md);
    CHECK(loud_frames(md, 60) == 0);   // quiet before anything is asked for
    int dead = 0, longest = 0;
    for (const SoundEntry& e : list) {
        m->play_sound(md, e.id);
        const int first = loud_frames(md, e.music ? 600 : 90);   // (some tunes open with a few quiet seconds)
        if (first == 0) { printf("  id %d (%s) makes no sound\n", e.id, e.name.c_str()); dead++; }
        if (e.music && e.seconds == 0) { longest = std::max(longest, first); CHECK(first > 120); }   // a level tune keeps going
        m->stop_sound(md);
        loud_frames(md, 60);
        CHECK(loud_frames(md, 30) == 0);   // stopped
    }
    CHECK(dead == 0);
}

int main(int argc, char** argv) {
    std::unique_ptr<GameModule> m1(create_ewj1_module()), m2(create_ewj2_module());
    if (argc > 1) test_game(argv[1], m1.get());
    if (argc > 2) test_game(argv[2], m2.get());
    if (failures == 0) printf("sound_test: all checks passed\n");
    return failures ? 1 : 0;
}
