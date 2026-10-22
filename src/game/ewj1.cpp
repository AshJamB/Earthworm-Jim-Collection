// Earthworm Jim (USA) - enhancement module.
// ROM identification, cheats and level select. Widescreen is added once it is reverse engineered;
// addresses and how they were found: docs/re_ewj1.md.
#include <algorithm>
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

// The game keeps its counters as ASCII decimal text and counts them up and down on the characters themselves.
constexpr uint32_t AMMO = 0xFFFBF4;      // 4 characters, "1000" at the start of a level (the plasma blaster)
constexpr uint32_t HEALTH = 0xFFFBFC;    // 4 characters, "0100" at full health
constexpr uint32_t LIVES = 0xFFA6C8;     // 1 character, '3' at the start of a game
constexpr uint32_t CAMERA_X = 0xFFA676;     // camera position in pixels (word); the left edge of the map is at 0
constexpr uint32_t WORLD_W = 0xFFA670;       // map width in pixels (word)
constexpr uint32_t LEVEL = 0xFFA692;     // level number (byte): the index into the game's own "START ON LEVEL" list

struct Stage { const char* group; const char* name; };   // list position == LEVEL value (the game steps through it with +1)
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

// Levels (LEVEL values) whose scenery does not go past the 4:3 screen: shown with black bars.
constexpr uint8_t kArenas[] = {3};

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

// Widescreen: the game draws its level map and spawns the enemies 22 map columns (352 px) right of the camera, just
// outside a 320 px screen. These move that to 24 columns (384 px) so scenery and enemies are in place before they
// enter the wider view; the rows drawn when the camera moves up or down are made 2 columns wider to match.
// Each patch is one immediate operand; docs/re_ewj1.md says where they are.
std::vector<RomPatch> widescreen_patches(const GameConfig& cfg) {
    if (cfg.ext <= 0) return {};
    return {
        {0x24817A, {0x0058}, {0x0060}},   // right column: VRAM column offset 44 -> 48 cells
        {0x2481A0, {0x002C}, {0x0030}},   // right column: map offset 22 -> 24 columns
        {0x2481B2, {0x0016}, {0x0018}},   // right column: stop 22 -> 24 columns before the map end
        {0x24AF10, {0x0150}, {0x0170}},   // enemies placed with the right column: x offset 336 -> 368
        {0x2482CA, {0x0016}, {0x0018}},   // row drawn below: 23 -> 25 columns
        {0x2483CA, {0x0016}, {0x0018}},   // row drawn above: 23 -> 25 columns
        {0x24AFCC, {0x0016}, {0x0018}},   // enemies placed with a new row: 23 -> 25 columns
    };
}

class EarthwormJim1 : public GameModule {
    EdgeFill fill_;
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
    int max_ext() const override { return 40; }   // 16:9; wider needs the games' scenery streaming widened

    void configure(MegaDrive& md, const GameConfig& cfg) override {
        apply_patches(md, widescreen_patches(cfg));
    }
    bool can_configure(const MegaDrive& md, const GameConfig& cfg) const override {
        return patches_safe(md, widescreen_patches(cfg));
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

    // A level is on screen while the health counter holds digits (the menus and cutscenes reuse that memory as other
    // data); only then does the app show the widescreen extension, elsewhere it stays black. Beyond the map's own
    // edges the extension is blacked out, and a few single-screen arenas (their scenery stops at the 4:3 edges) keep
    // black bars on both sides.
    void post_frame(MegaDrive& md, const GameConfig& cfg, FrameInfo& info) override {
        info.gameplay = is_digits(md, HEALTH, 4) && md.ram8(LIVES) >= '0' && md.ram8(LIVES) <= '9';
        if (!info.gameplay || cfg.ext <= 0) return;
        const uint8_t level = md.ram8(LEVEL);
        for (uint8_t a : kArenas)
            if (level == a) { info.mask_left = info.mask_right = cfg.ext; return; }
        const int cam = md.ram16(CAMERA_X), world = md.ram16(WORLD_W);
        info.mask_left = std::clamp(cfg.ext - cam, 0, cfg.ext);
        info.mask_right = world > 0 ? std::clamp(cam + 320 + cfg.ext - world, 0, cfg.ext) : 0;
    }

    // Applied before every frame. Each write is made only while the game shows that counter as digits, so the
    // title and menu screens (which reuse the memory) are left alone.
    void pre_frame(MegaDrive& md, const GameConfig& cfg) override {
        fill_.run(md, cfg.ext, true);   // widescreen: carry the background past its own edges
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
