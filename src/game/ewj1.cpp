// Earthworm Jim (USA) - enhancement module.
// ROM identification, cheats and level select. Widescreen is added once it is reverse engineered;
// addresses and how they were found: docs/re_ewj1.md.
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

// The game keeps its counters as ASCII decimal text and counts them up and down on the characters themselves.
constexpr uint32_t AMMO = 0xFFFBF4;      // 4 characters, "1000" at the start of a level (the plasma blaster)
constexpr uint32_t HEALTH = 0xFFFBFC;    // 4 characters, "0100" at full health
constexpr uint32_t LIVES = 0xFFA6C8;     // 1 character, '3' at the start of a game
constexpr uint32_t LEVEL = 0xFFA692;     // level number (byte): the index into the game's own "START ON LEVEL" list

struct Stage { const char* group; const char* name; };
// In the order of the developers' hidden debug menu (David Perry's Private Cheat Mode, ROM $1B6722); the index is
// the value of LEVEL. Values above 28 load nothing.
const Stage kStages[] = {
    {"Level 1 - New Junk City", "1  New Junk City"},
    {"Level 2 - Hell or Heck?", "2A  Hell or Heck?"},
    {"Level 2 - Hell or Heck?", "2B  Snowman Boss"},
    {"Level 2 - Hell or Heck?", "2C  Evil Boss"},
    {"Level 3 - Bungee", "3A  Bungee 1"},
    {"Level 3 - Bungee", "3B  Bungee 2"},
    {"Level 3 - Bungee", "3C  Bungee 3"},
    {"Level 4 - Peter Puppy", "4  Peter Puppy"},
    {"Level 5 - Slug for a Butt", "5  Slug for a Butt"},
    {"Level 5 - Slug for a Butt", "5B  Helicopter"},
    {"Level 6 - Professor's Lab", "6A  Prof's Lab"},
    {"Level 6 - Professor's Lab", "6C  Chicken Fly"},
    {"Level 6 - Professor's Lab", "6D  Naked Worm"},
    {"Level 7 - Intestines", "7  Intestines"},
    {"Level 8 - Sea Tunnels", "8A  Sea Tunnels"},
    {"Level 8 - Sea Tunnels", "8B  Pod Races"},
    {"Level 9 - Asteroids", "9A  Asteroids 1"},
    {"Level 9 - Asteroids", "9B  Asteroids 2"},
    {"Level 9 - Asteroids", "9C  Asteroids 3"},
    {"Level 9 - Asteroids", "9D  Asteroids 4"},
    {"Level 9 - Asteroids", "9E  Asteroids 5"},
    {"Level 9 - Asteroids", "9F  Asteroids 6"},
    {"Level 9 - Asteroids", "9G  Asteroids 7"},
    {"Level 10 - Darkness", "10A  Darkness 1"},
    {"Level 10 - Darkness", "10B  Darkness 2"},
    {"Level 10 - Darkness", "10C  Darkness 3"},
    {"Level 10 - Darkness", "10D  Darkness 4"},
    {"Level 10 - Darkness", "10E  Darkness 5"},
    {"Level 9P - Psy-Crow", "9P  Psy-Crow!"},
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

class EarthwormJim1 : public GameModule {
public:
    const char* id() const override { return "ewj1"; }
    const char* title() const override { return "Earthworm Jim"; }
    const char* subtitle() const override { return "Sega Genesis"; }
    const char* year() const override { return "1994"; }
    uint32_t rom_crc32() const override { return 0xDF3ACF59; }   // Earthworm Jim (USA), 3 MB
    uint32_t rom_size() const override { return 0x300000; }
    const char* rom_hint() const override { return "the Earthworm Jim (USA) ROM"; }
    const char* rom_names() const override { return "=EARTHWORMJIM"; }
    uint32_t accent_color() const override { return 0xFFE8B84A; }
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

    // Plays the game's own start-up from power-on (SEGA logo, title, "press start") and, just before the START that
    // begins the level, writes the stage number: the game's loader then takes that stage as if it were the first.
    bool start_level(MegaDrive& md, const GameConfig& cfg, int index, int variant) override {
        (void)cfg; (void)variant;
        if (index < 0 || index >= kStageCount) return false;
        md.power_on();
        auto run = [&](int n) { for (int i = 0; i < n; i++) md.run_frame(); };
        auto tap = [&](int after) { md.set_pad(0, MD_START); run(4); md.set_pad(0, 0); run(after); };
        run(400);
        tap(150);
        tap(60);
        md.set_ram8(LEVEL, (uint8_t)index);
        tap(600);
        return true;
    }

    // Applied before every frame. Each write is made only while the game shows that counter as digits, so the
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

GameModule* create_ewj1_module() { return new EarthwormJim1(); }
