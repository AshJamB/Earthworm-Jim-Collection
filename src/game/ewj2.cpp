// Earthworm Jim 2 (USA) - enhancement module.
// ROM identification and cheats. Widescreen and level select are added as they are reverse engineered;
// addresses and how they were found: docs/re_ewj2.md.
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

// Same scheme as the first game: counters are ASCII decimal text that the game counts on directly.
constexpr uint32_t AMMO = 0xFFFADE;      // 4 characters, "1000" at the start of a level (the plasma blaster)
constexpr uint32_t HEALTH = 0xFFFAFC;    // 4 characters, "0100" at full health
constexpr uint32_t LIVES = 0xFFA1EA;     // 1 character, '3' at the start of a game

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
