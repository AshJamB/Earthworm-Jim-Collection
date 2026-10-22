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
// The level map layer as the game draws it (see StreamLayer): camera snapped to 16 px, VDP address table, map row
// pointers (longs), row stride and metatile table. Used to keep the cells left of the screen up to date.
const StreamLayer kMapLayer = {0xFFA686, 0xFFA688, 0xFFB396, 0xFFB548, true, 2, 0xFFA66C, 0xFFA6B8, 0};
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
// The status bar (lives, health, ammo) is a run of sprites the game writes into its sprite table buffer ($FF9B52 up to
// A1) before the objects are added, all of them left of the screen's middle with fixed x values. With HUD anchoring on,
// the join point after them ($2488E2, `lea $FFA6D2,a2`) calls a stub in free ROM that moves each of those sprites `ext`
// px left and sets bit 15 of its y (the VDP ignores it; the app takes it as "status bar sprite" and keeps it visible
// beside the level edges), then does the lea.
void hud_patches(std::vector<RomPatch>& p, const GameConfig& cfg) {
    // the two builders' fixed-position branches ($248A00 / $248B60) keep the wider cull
    p.push_back({0x248A06, {0x01BF}, {0x01EF}});
    p.push_back({0x248B66, {0x01BF}, {0x01EF}});
    if (!cfg.hud_anchor) return;
    const std::vector<uint16_t> stub = {0x45F9, 0x00FF, 0x9B52, 0xB5C9, 0x640E, 0x046A, (uint16_t)cfg.ext, 0x0006,
                                        0x0052, 0x8000, 0x508A, 0x60EE, 0x45F9, 0x00FF, 0xA6D2, 0x4E75};
    p.push_back({0x2FFC40, std::vector<uint16_t>(stub.size(), 0xFFFF), stub});
    p.push_back({0x2488E2, {0x45F9, 0x00FF, 0xA6D2}, {0x4EB9, 0x002F, 0xFC40}});
}

std::vector<RomPatch> widescreen_patches(const GameConfig& cfg) {
    if (cfg.ext <= 0) return {};
    std::vector<RomPatch> p = {
        {0x24817A, {0x0058}, {0x0060}},   // right column: VRAM column offset 44 -> 48 cells
        {0x2481A0, {0x002C}, {0x0030}},   // right column: map offset 22 -> 24 columns
        {0x2481B2, {0x0016}, {0x0018}},   // right column: stop 22 -> 24 columns before the map end
        {0x24AF10, {0x0150}, {0x0170}},   // enemies placed with the right column: x offset 336 -> 368
        {0x2482CA, {0x0016}, {0x0018}},   // row drawn below: 23 -> 25 columns
        {0x2483CA, {0x0016}, {0x0018}},   // row drawn above: 23 -> 25 columns
        {0x24AFCC, {0x0016}, {0x0018}},   // enemies placed with a new row: 23 -> 25 columns
        // Enemies and items of the column that scrolls in on the left are placed 3 columns further out, where the 16 px
        // margin of the original is now inside the view (an item popped into view at the edge): the map pointer of
        // that scan (the store at $2480B4) goes through a stub in free ROM that backs it up 3 columns (not near the
        // left end of the map), and the x offset $FFF0 (-16) becomes -64.
        {0x2480B4, {0x23C8, 0x00FF, 0xA694}, {0x4EB9, 0x002F, 0xFC00}},
        {0x2FFC00, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, {0x2F08, 0x0C79, 0x0030, 0x00FF, 0xA686, 0x6502, 0x5D88, 0x23C8, 0x00FF, 0xA694, 0x205F, 0x4E75}},
        {0x24AF06, {0xFFF0}, {0xFFC0}},
        // Sprite pieces right of the screen are not sent to the VDP (x < $1BF, 447 = 319 px on screen): allow 48 px more.
        {0x248A50, {0x01BF}, {0x01EF}}, {0x248BB0, {0x01BF}, {0x01EF}},   // objects placed on the map
    };
    hud_patches(p, cfg);
    return p;
}

class EarthwormJim1 : public GameModule {
    EdgeFill fill_;
    uint8_t fill_level_ = 0xFF;   // the level fill_ has been following
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
        if (cfg.ext > 0 && is_digits(md, HEALTH, 4)) refresh_edge_strips(md, kMapLayer, cfg.ext);   // widescreen: the strips beside the screen
        // widescreen: carry the background past its own edges (start afresh in each level and outside levels)
        if (is_digits(md, HEALTH, 4) && md.ram8(LEVEL) == fill_level_) fill_.run(md, cfg.ext, true);
        else { fill_.reset(); fill_level_ = md.ram8(LEVEL); }
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
