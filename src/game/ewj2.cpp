// Earthworm Jim 2 (USA) - enhancement module.
// ROM identification, cheats and level select. Widescreen is added once it is reverse engineered;
// addresses and how they were found: docs/re_ewj2.md.
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

// Same scheme as the first game: counters are ASCII decimal text that the game counts on directly.
constexpr uint32_t AMMO = 0xFFFADE;      // 4 characters, "1000" at the start of a level (the plasma blaster)
constexpr uint32_t HEALTH = 0xFFFAFC;    // 4 characters, "0100" at full health
constexpr uint32_t LIVES = 0xFFA1EA;     // 1 character, '3' at the start of a game
constexpr uint32_t LEVEL = 0xFFA1A6;     // level number (byte): the index into the game's own "START ON LEVEL" list

struct Stage { const char* group; const char* name; uint8_t value; };
// In play order, taken from the game's own level script (ROM $27346E, read by the loader at $24EE50). `value` is what
// the game stores in LEVEL for that stage: the index into its hidden debug list (ROM $1C0A1D), which is NOT play order.
// Values above 16 load nothing.
const Stage kStages[] = {
    {"Level 1 - Anything but Tangerines", "1  Anything but Tangerines", 0},
    {"Level 1 - Anything but Tangerines", "1B  Granny Chair", 1},
    {"Level 2 - Lorenzen's Soil", "2  Lorenzen's Soil", 10},
    {"Level 3 - Puppy Love", "3A  Puppy Love 1", 2},
    {"Level 4 - Villi People", "4  Villi People", 9},
    {"Level 5 - The Flyin' King", "5  The Flyin' King", 5},
    {"Level 5 - The Flyin' King", "5B  Puppy Love 2", 3},
    {"Level 6 - Udderly Abducted", "6  Udderly Abducted", 6},
    {"Level 7 - Inflated Head", "7A  Inflated Head", 7},
    {"Level 7 - Inflated Head", "7B  Hammer Head", 8},
    {"Level 8 - ISO 9000", "8A  ISO 9000", 14},
    {"Level 8 - ISO 9000", "8B  Door Chase", 15},
    {"Level 8 - ISO 9000", "8C  Puppy Love 3", 4},
    {"Level 9 - Level Ate", "9A  Level Ate", 11},
    {"Level 9 - Level Ate", "9B  Forked", 13},
    {"Level 9 - Level Ate", "9C  Flamin' Yawn", 12},
    {"Level 10 - See Jim Run", "10  See Jim Run", 16},
};
constexpr int kStageCount = (int)(sizeof(kStages) / sizeof(kStages[0]));

bool is_digits(const MegaDrive& md, uint32_t a, int n) {
    for (int i = 0; i < n; i++) {
        const uint8_t c = md.ram8(a + i);
        if (c < '0' || c > '9') return false;
    }
    return true;
}

void put_text(MegaDrive& md, uint32_t a, const char* s) {
    for (int i = 0; s[i]; i++) md.set_ram8(a + i, (uint8_t)s[i]);
}

class EarthwormJim2 : public GameModule {
public:
    const char* id() const override { return "ewj2"; }
    const char* title() const override { return "Earthworm Jim 2"; }
    const char* subtitle() const override { return "Sega Genesis"; }
    const char* year() const override { return "1995"; }
    uint32_t rom_crc32() const override { return 0xD57F8BA7; }   // Earthworm Jim 2 (USA), 3 MB
    uint32_t rom_size() const override { return 0x300000; }
    const char* rom_hint() const override { return "the Earthworm Jim 2 (USA) ROM"; }
    const char* rom_names() const override { return "=EARTHWORMJIM2"; }
    uint32_t accent_color() const override { return 0xFF5BC0EB; }
    int max_ext() const override { return 0; }   // no widescreen until it is reverse engineered

    void configure(MegaDrive& md, const GameConfig& cfg) override {
        (void)md; (void)cfg;   // no ROM patches yet: cheats work on RAM
    }

    std::vector<CheatEntry> cheats() const override {
        return {
            {"inf_health", "Infinite Health", "Jim's suit energy stays at 100%. Falling into a pit still costs a life.", {}},
            {"inf_lives", "Infinite Lives", "The lives counter stays at 9.", {}},
            {"inf_ammo", "Infinite Ammo", "The plasma blaster never runs out.", {}},
        };
    }

    std::vector<LevelEntry> levels() const override {
        std::vector<LevelEntry> v;
        for (const Stage& s : kStages) v.push_back({s.group, s.name, 0});
        return v;
    }

    // Plays the game's own start-up from power-on (logos, title, "press start") and, just before the START that
    // begins the level, writes the stage number: the game's loader then takes that stage as if it were the first.
    bool start_level(MegaDrive& md, const GameConfig& cfg, int index, int variant) override {
        (void)cfg; (void)variant;
        if (index < 0 || index >= kStageCount) return false;
        md.power_on();
        auto run = [&](int n) { for (int i = 0; i < n; i++) md.run_frame(); };
        auto tap = [&](int after) { md.set_pad(0, MD_START); run(4); md.set_pad(0, 0); run(after); };
        run(400);
        tap(150);
        tap(150);
        tap(150);
        run(60);
        md.set_ram8(LEVEL, kStages[index].value);
        tap(500);
        return true;
    }

    // Applied before every frame. Each write is made only while the game shows its counters as digits, so the
    // title and menu screens (which reuse the memory) are left alone.
    void pre_frame(MegaDrive& md, const GameConfig& cfg) override {
        const bool hud = is_digits(md, HEALTH, 4);
        const bool dead = hud && md.ram8(HEALTH) == '0' && md.ram8(HEALTH + 1) == '0' && md.ram8(HEALTH + 2) == '0' &&
                          md.ram8(HEALTH + 3) == '0';   // already at 0%: the death has started, leave it
        if (cfg.cheat("inf_health") && hud && !dead)
            put_text(md, HEALTH, "0100");
        if (cfg.cheat("inf_lives") && hud && md.ram8(LIVES) >= '0' && md.ram8(LIVES) <= '9')
            md.set_ram8(LIVES, '9');
        if (cfg.cheat("inf_ammo") && hud && is_digits(md, AMMO, 4))
            put_text(md, AMMO, "1000");
    }
};

}  // namespace

GameModule* create_ewj2_module() { return new EarthwormJim2(); }
