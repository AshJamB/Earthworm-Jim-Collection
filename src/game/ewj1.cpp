// Earthworm Jim (USA) - enhancement module.
// Stage 1: ROM identification only. Patches (widescreen, cheats, level select) are added as they are
// reverse engineered; see docs/.
#include "game.h"
#include "common.h"
#include "../core/md.h"

namespace {

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
        (void)md; (void)cfg;   // no patches yet: the ROM runs as dumped
    }
};

}  // namespace

GameModule* create_ewj1_module() { return new EarthwormJim1(); }
