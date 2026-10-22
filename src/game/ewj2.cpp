// Earthworm Jim 2 (USA) - enhancement module.
// Stage 1: ROM identification only. Patches (widescreen, cheats, level select) are added as they are
// reverse engineered; see docs/.
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

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

    void configure(MegaDrive& md, const GameConfig& cfg) override {
        (void)md; (void)cfg;   // no patches yet: the ROM runs as dumped
    }
};

}  // namespace

GameModule* create_ewj2_module() { return new EarthwormJim2(); }
