// Earthworm Jim (USA) - enhancement module.
// ROM identification and cheats. Widescreen and level select are added as they are reverse engineered;
// addresses and how they were found: docs/re_ewj1.md.
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

// The game keeps its counters as ASCII decimal text and counts them up and down on the characters themselves.
constexpr uint32_t AMMO = 0xFFFBF4;      // 4 characters, "1000" at the start of a level (the plasma blaster)
constexpr uint32_t HEALTH = 0xFFFBFC;    // 4 characters, "0100" at full health
constexpr uint32_t LIVES = 0xFFA6C8;     // 1 character, '3' at the start of a game

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
