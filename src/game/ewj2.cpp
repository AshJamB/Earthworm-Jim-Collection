// Earthworm Jim 2 (USA) - enhancement module.
// ROM identification, cheats and level select. Widescreen is added once it is reverse engineered;
// addresses and how they were found: docs/re_ewj2.md.
#include <algorithm>
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

// Same scheme as the first game: counters are ASCII decimal text that the game counts on directly.
constexpr uint32_t AMMO = 0xFFFADE;      // 4 characters, "1000" at the start of a level (the plasma blaster)
constexpr uint32_t HEALTH = 0xFFFAFC;    // 4 characters, "0100" at full health
constexpr uint32_t LIVES = 0xFFA1EA;     // 1 character, '3' at the start of a game
constexpr uint32_t CAMERA_X = 0xFFA172;     // camera position in pixels (word); the left edge of the map is at 0
constexpr uint32_t WORLD_W = 0xFFA16C;       // map width in pixels (word)
// The two scrolling map layers as the game draws them (see StreamLayer). Used to keep the cells left of the screen up
// to date; plane B only while the game streams it (RAM $FFFBB2), neither while it has map drawing switched off ($FFFF15).
const StreamLayer kMapLayerA = {0xFFA182, 0xFFA184, 0xFFB262, 0xFFB4E6, false, 3, 0xFFA166, 0xFFA1D0, 0xFF9416};
const StreamLayer kMapLayerB = {0xFFA18E, 0xFFA190, 0xFFB362, 0xFFB676, false, 3, 0xFFA166, 0xFFA1D0, 0xFF9416};
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

// Levels (LEVEL values) whose scenery does not go past the 4:3 screen: shown with black bars.
constexpr uint8_t kArenas[] = {1, 7, 8, 12};

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

// Widescreen: like the first game, this one draws its map and places its enemies 22 map columns (352 px) right of the
// camera, just outside a 320 px screen. These move that to 24 columns (384 px), and make the rows drawn when the
// camera moves up or down 2 columns wider. The engine runs two scrolling layers, each with its own copy of the code
// (plane A: $2530xx / $2531xx / $2532xx / $2533xx, plane B: $2534xx - $2537xx). Each patch is one immediate operand;
// docs/re_ewj2.md says where they are.
std::vector<RomPatch> widescreen_patches(const GameConfig& cfg) {
    if (cfg.ext <= 0) return {};
    return {
        {0x253094, {0x0058}, {0x0060}},   // plane A right column: VRAM column offset 44 -> 48 cells
        {0x2530C0, {0x002C}, {0x0030}},   //   map offset 22 -> 24 columns
        {0x2530D2, {0x0016}, {0x0018}},   //   stop 22 -> 24 columns before the map end
        {0x25651C, {0x1150}, {0x1170}},   //   enemies placed with the column: x offset 336 -> 368
        {0x25320C, {0x0016}, {0x0018}},   // plane A row drawn below: 23 -> 25 columns
        {0x25332C, {0x0016}, {0x0018}},   // plane A row drawn above: 23 -> 25 columns
        {0x2565FC, {0x0016}, {0x0018}},   //   enemies placed with a new row: 23 -> 25 columns
        {0x253516, {0x0058}, {0x0060}},   // plane B right column: VRAM column offset 44 -> 48 cells
        {0x253542, {0x002C}, {0x0030}},   //   map offset 22 -> 24 columns
        {0x253554, {0x0016}, {0x0018}},   //   stop 22 -> 24 columns before the map end
        {0x25367E, {0x0016}, {0x0018}},   // plane B row drawn below: 23 -> 25 columns
        {0x253794, {0x0016}, {0x0018}},   // plane B row drawn above: 23 -> 25 columns
        // Enemies and items of the column that scrolls in on the left are placed 3 columns further out (see ewj1.cpp): the
        // map pointer store at $252FB6 goes through a stub in free ROM, the x offset $0FF0 becomes $0FC0.
        {0x252FB6, {0x23C8, 0x00FF, 0xA1A8}, {0x4EB9, 0x002F, 0xEE00}},
        {0x2FEE00, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, {0x2F08, 0x0C79, 0x0030, 0x00FF, 0xA182, 0x6502, 0x5D88, 0x23C8, 0x00FF, 0xA1A8, 0x205F, 0x4E75}},
        {0x256512, {0x0FF0}, {0x0FC0}},
        // Sprite pieces right of the screen are not sent to the VDP (x < $1BF, 447 = 319 px on screen): allow 48 px more.
        {0x255CF2, {0x01BF}, {0x01EF}}, {0x255D54, {0x01BF}, {0x01EF}}, {0x255DB6, {0x01BF}, {0x01EF}}, {0x255E1E, {0x01BF}, {0x01EF}},
        {0x255E7E, {0x01BF}, {0x01EF}}, {0x255EE6, {0x01BF}, {0x01EF}}, {0x255F4E, {0x01BF}, {0x01EF}}, {0x255FBC, {0x01BF}, {0x01EF}},
    };
}

class EarthwormJim2 : public GameModule {
    EdgeFill fill_;
    uint8_t fill_level_ = 0xFF;   // the level fill_ has been following
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

    // Applied before every frame. Each write is made only while the game shows its counters as digits, so the
    // title and menu screens (which reuse the memory) are left alone.
    void pre_frame(MegaDrive& md, const GameConfig& cfg) override {
        if (cfg.ext > 0 && is_digits(md, HEALTH, 4) && md.ram8(0xFFFF15) == 0) {   // widescreen: the strips beside the screen
            refresh_edge_strips(md, kMapLayerA, cfg.ext);
            if (md.ram8(0xFFFBB2)) refresh_edge_strips(md, kMapLayerB, cfg.ext);
        }
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

GameModule* create_ewj2_module() { return new EarthwormJim2(); }
