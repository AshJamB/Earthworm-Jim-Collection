#include "common.h"
#include "../core/md.h"
#include <algorithm>
#include <unordered_map>

int apply_patches(MegaDrive& md, const std::vector<RomPatch>& patches) {
    md.restore_rom();
    int applied = 0;
    for (const RomPatch& p : patches) {
        bool ok = true;
        for (size_t i = 0; i < p.orig.size(); i++)
            if (md.rom16(p.addr + (uint32_t)i * 2) != p.orig[i]) { ok = false; break; }
        if (!ok) continue;
        for (size_t i = 0; i < p.repl.size(); i++) md.patch_rom16(p.addr + (uint32_t)i * 2, p.repl[i]);
        applied++;
    }
    if (applied) {
        // the games' boot test sums $200 up to the ROM end the header gives ($1A4), not the whole ROM: an MD+
        // patch can append code beyond it
        uint32_t end = md.rom32(0x1A4) + 1;
        if (end <= 0x200 || end > md.rom.size()) end = (uint32_t)md.rom.size();
        uint16_t sum = 0;
        for (uint32_t a = 0x200; a + 1 < end; a += 2) sum = (uint16_t)(sum + md.rom16(a));
        md.patch_rom16(0x18E, sum);
    }
    return applied;
}

bool patches_safe(const MegaDrive& md, const std::vector<RomPatch>& patches) {
    // the ROM the patches give (applied in order on the pristine ROM, as apply_patches does)
    std::unordered_map<uint32_t, uint16_t> next;
    auto word = [&](uint32_t a) { auto i = next.find(a); return i != next.end() ? i->second : md.rom_orig16(a); };
    for (const RomPatch& p : patches) {
        bool ok = true;
        for (size_t i = 0; i < p.orig.size() && ok; i++) ok = word(p.addr + (uint32_t)i * 2) == p.orig[i];
        if (!ok) continue;
        for (size_t i = 0; i < p.repl.size(); i++) next[p.addr + (uint32_t)i * 2] = p.repl[i];
    }
    std::vector<uint32_t> changed;
    for (uint32_t a = 0x200; a + 1 < md.rom.size(); a += 2)
        if (word(a) != md.rom16(a)) changed.push_back(a);
    if (changed.empty()) return true;
    auto hit = [&](uint32_t lo, uint32_t hi) {      // a changed word in [lo, hi)
        auto i = std::lower_bound(changed.begin(), changed.end(), lo);
        return i != changed.end() && *i < hi;
    };
    const uint32_t pc = md.cpu.pc & 0xFFFFFF;
    if (hit(pc, pc + 10)) return false;
    // return addresses (jsr / bsr, interrupt frames) in the top of the stack
    const uint32_t sp = md.cpu.r[15] & 0xFFFFFF;
    if (sp >= 0xFF0000)
        for (uint32_t o = 0; o < 512 && sp + o + 3 <= 0xFFFFFF; o += 2) {
            const uint32_t a = (sp + o) & 0xFFFF;
            const uint32_t v = ((uint32_t)md.ram[a] << 24 | (uint32_t)md.ram[(a + 1) & 0xFFFF] << 16 |
                                (uint32_t)md.ram[(a + 2) & 0xFFFF] << 8 | md.ram[(a + 3) & 0xFFFF]) & 0xFFFFFF;
            if (v >= 0x200 && hit(v, v + 8)) return false;
        }
    return true;
}

bool run_until(MegaDrive& md, const std::function<bool()>& cond, int max_frames) {
    for (int i = 0; i < max_frames; i++) {
        if (cond()) return true;
        md.run_frame();
    }
    return cond();
}

bool press_until(MegaDrive& md, uint16_t button, const std::function<bool()>& cond,
                 const std::function<bool()>& idle, int hold_frames, int tries) {
    for (int t = 0; t < tries; t++) {
        md.set_pad(0, 0);
        if (!run_until(md, idle, 600)) return false;
        md.set_pad(0, button);
        for (int f = 0; f < hold_frames; f++) {
            md.run_frame();
            if (cond()) { md.set_pad(0, 0); return true; }
        }
        md.set_pad(0, 0);
        md.run_frame();
    }
    return false;
}

void freeze_68k(MegaDrive& md) {
    md.cpu.set_sr(0x2700);
    md.cpu.stopped = true;
    md.z80_busreq = false;
    md.z80_reset = false;
}

namespace {
// The return address call_68k pushes: the vector table, never code. Reaching it ends the call before it executes.
constexpr uint32_t kCallReturn = 0x000000;
void call_trace(void* ctx, uint32_t pc) {
    M68k& c = *(M68k*)ctx;
    if ((pc & 0xFFFFFF) == kCallReturn) c.end_run();
}
}  // namespace

bool call_68k(MegaDrive& md, uint32_t entry, int max_cycles) {
    M68k& c = md.cpu;
    const M68k::HookFn trace = c.trace_fn;
    void* const trace_ctx = c.trace_ctx;
    const uint32_t sp = c.r[15];
    c.r[15] = sp - 4;
    c.write32(c.r[15], kCallReturn);
    c.pc = entry & 0xFFFFFF;
    c.set_sr(0x2700);
    c.stopped = false;
    c.trace_fn = call_trace;
    c.trace_ctx = &c;
    int used = 0;
    while ((c.pc & 0xFFFFFF) != kCallReturn && used < max_cycles) {
        const int n = c.run(4000);
        used += n;
        if (n == 0 && (c.pc & 0xFFFFFF) != kCallReturn) break;   // (stopped by the routine)
    }
    const bool ok = (c.pc & 0xFFFFFF) == kCallReturn;
    c.trace_fn = trace;
    c.trace_ctx = trace_ctx;
    c.r[15] = sp;
    freeze_68k(md);
    return ok;
}

void HudLayout::build(int ext, int width) {
    const int total = width + ext * 2;
    map.assign(total, -1);
    // Default: centred copy of the original bar.
    for (int x = 0; x < width; x++) map[x + ext] = (int16_t)x;
    if (ext > 0) {
        // Fill everything with the filler pattern first, then place the segments.
        int fw = std::max(1, filler_x1 - filler_x0);
        for (int x = 0; x < total; x++) map[x] = (int16_t)(filler_x0 + ((x - ext) % fw + fw) % fw);
        for (const HudSegment& s : segments) {
            int shift = s.anchor < 0 ? -ext : s.anchor > 0 ? ext : 0;
            for (int sx = s.src_x0; sx < s.src_x1; sx++) {
                int ox = sx + ext + shift;
                if (ox >= 0 && ox < total) map[ox] = (int16_t)sx;
            }
        }
    }
    built_ext = ext;
}

void hud_remap_window_line(VDP& vdp, int line, uint8_t* plane_a, uint8_t* plane_b, const std::vector<int16_t>& map) {
    const int ext = vdp.ext;
    const int width = vdp.active_width();
    uint8_t src[VDP::kMaxWidth];
    vdp.render_window(src, line, 0, width);
    const int total = std::min<int>(width + ext * 2, (int)map.size());
    for (int x = 0; x < total; x++) {
        int sx = map[x];
        plane_a[x] = sx >= 0 && sx < width ? src[sx] : 0;
        if (x < ext || x >= ext + width) plane_b[x] = 0;
    }
}

void EdgeFill::run(MegaDrive& md, int ext, bool plane_b) {
    VDP& v = md.vdp;
    auto restore = [&]() {
        for (const Saved& s : saved_) {   // only the cells the game has not drawn over since
            const uint16_t now = (uint16_t)((v.vram[s.addr] << 8) | v.vram[(s.addr + 1) & 0xFFFF]);
            if (now == s.put) { v.vram[s.addr] = (uint8_t)(s.orig >> 8); v.vram[(s.addr + 1) & 0xFFFF] = (uint8_t)s.orig; }
        }
        saved_.clear();
    };
    if (ext <= 0 || (v.reg[11] & 3) != 0 || (v.reg[16] & 3) != 1) { restore(); still_ = 0; last_scroll_ = -1; return; }
    const uint32_t base = plane_b ? (uint32_t)(v.reg[4] & 7) << 13 : (uint32_t)(v.reg[2] & 0x38) << 10;
    const uint32_t hs = ((uint32_t)(v.reg[13] & 0x3F) << 10) + (plane_b ? 2 : 0);
    const int scroll = ((v.vram[hs] << 8) | v.vram[hs + 1]) & 0x3FF;
    still_ = scroll == last_scroll_ ? std::min(still_ + 1, 1000) : 0;
    last_scroll_ = scroll;
    const bool picture = still_ >= 8;
    if (!picture && !saved_.empty()) restore();
    const int c0 = (((-scroll) & 0x3FF) >> 3) & 63;                      // first visible cell column
    const int n = (ext + 7) / 8 + 1;                                     // cells to fill on each side
    const int rows = 32;
    auto addr = [&](int col, int row) { return (base + (uint32_t)((row * 64 + (col & 63)) << 1)) & 0xFFFF; };
    auto cell = [&](int col, int row) -> uint16_t {
        const uint32_t a = addr(col, row);
        return (uint16_t)((v.vram[a] << 8) | v.vram[(a + 1) & 0xFFFF]);
    };
    auto put = [&](int col, int row, uint16_t x) {
        const uint32_t a = addr(col, row);
        v.vram[a] = (uint8_t)(x >> 8);
        v.vram[(a + 1) & 0xFFFF] = (uint8_t)x;
    };
    auto empty = [&](int col) { for (int r = 0; r < rows; r++) if (cell(col, r) & 0x7FF) return false; return true; };
    auto full = [&](int col) { for (int r = 0; r < 28; r++) if (!(cell(col, r) & 0x7FF)) return false; return true; };
    auto mirror = [&](int dst, int src, bool keep) {
        for (int r = 0; r < rows; r++) {
            const uint16_t want = (uint16_t)(cell(src, r) ^ 0x0800), cur = cell(dst, r);
            if (want == cur) continue;
            if (keep) {
                bool known = false;
                for (const Saved& s : saved_) if (s.addr == addr(dst, r)) { known = true; break; }
                if (!known) saved_.push_back({addr(dst, r), cur, want});
                else for (Saved& s : saved_) if (s.addr == addr(dst, r)) s.put = want;
            }
            put(dst, r, want);
        }
    };
    for (int j = 0; j < n; j++) {
        const int dl = c0 - 1 - j, sl = c0 + j;
        const int dr = c0 + 40 + j, sr = c0 + 39 - j;
        if (picture) {
            if (full(sl)) mirror(dl, sl, true);
            if (full(sr)) mirror(dr, sr, true);
        } else {
            if (empty(dl) && full(sl)) mirror(dl, sl, false);
            if (empty(dr) && full(sr)) mirror(dr, sr, false);
        }
    }
}

int refresh_left_columns(MegaDrive& md, const StreamLayer& L, int cols, bool verify, int first, int dir) {
    auto r16 = [&](uint32_t a) { return (uint32_t)md.ram16(a); };
    auto r32 = [&](uint32_t a) { return md.ram32(a); };
    VDP& v = md.vdp;
    const int snap_x = (int16_t)r16(L.snap_x), snap_y = (uint16_t)r16(L.snap_y);
    const uint32_t tiles = r32(L.tiles), limit = L.limit ? r32(L.limit) : 0;
    const int stride = (int16_t)r16(L.stride);
    int diffs = 0;
    for (int k = 0; k < cols; k++) {
        const int x = snap_x + dir * 16 * (first + k);
        if (x < 0 || x > 0x7FF0) continue;
        uint32_t p;
        if (L.rowptr_is_long) p = r32(L.rowptr_tab + ((snap_y & 0xFFF0) >> L.rowptr_shift));
        else p = 0xFF0000u | r16(L.rowptr_tab + ((snap_y & 0xFFF0) >> L.rowptr_shift));
        p += (uint32_t)((x >> 4) * 2);
        const uint32_t d1 = (uint32_t)(x >> 2) & 0x7E, d5 = (d1 + 2) & 0x7E;
        uint32_t tab = L.vram_tab + (uint32_t)(((snap_y >> 3) & 0x1F) * 4);
        for (int i = 0; i < 16; i++) {
            uint32_t d4 = 0;
            if (!limit || p < limit) d4 = md.read16(p) & 0xFFFF;
            const uint32_t t = tiles + (uint32_t)(int16_t)d4;
            const uint32_t e0 = r32(tab), e1 = r32(tab + 4);
            const struct { uint32_t e, d, off; } cell[4] = {{e0, d1, 0}, {e0, d5, 2}, {e1, d1, 4}, {e1, d5, 6}};
            for (const auto& c : cell) {
                const uint32_t lo = (c.e | c.d) & 0xFFFF, hi = (c.e >> 16) & 0xFFFF;
                const uint32_t addr = (lo & 0x3FFF) | ((hi & 3) << 14);
                const uint16_t w = md.read16(t + c.off);
                const uint16_t cur = (uint16_t)((v.vram[addr] << 8) | v.vram[(addr + 1) & 0xFFFF]);
                if (verify) { if (cur != w) diffs++; }
                else { v.vram[addr] = (uint8_t)(w >> 8); v.vram[(addr + 1) & 0xFFFF] = (uint8_t)w; }
            }
            tab += 8;
            p += (uint32_t)(int32_t)stride;
        }
    }
    return diffs;
}

bool refresh_left_strip(MegaDrive& md, const StreamLayer& layer, int cols) {
    if (refresh_left_columns(md, layer, 2, true, 1, +1) != 0) return false;
    refresh_left_columns(md, layer, cols);
    return true;
}

bool refresh_edge_strips(MegaDrive& md, const StreamLayer& layer, int ext) {
    if (refresh_left_columns(md, layer, 2, true, 1, +1) != 0) return false;
    const int n = (ext + 15) / 16;
    refresh_left_columns(md, layer, n);                       // left of the view
    refresh_left_columns(md, layer, n + 3, false, 19, +1);    // right of the view (from the last columns inside it)
    return true;
}

void hud_remap_line(VDP& vdp, int line, uint8_t* plane_a, uint8_t* plane_b, HudLayout& layout) {
    if (line < layout.y0 || line >= layout.y1) return;
    const int ext = vdp.ext;
    const int width = vdp.active_width();
    if (layout.built_ext != ext) layout.build(ext, width);
    uint8_t src[VDP::kMaxWidth];
    vdp.render_window(src, line, 0, width);
    const int total = width + ext * 2;
    for (int x = 0; x < total; x++) {
        int sx = layout.map[x];
        plane_a[x] = sx >= 0 ? src[sx] : 0;
        if (layout.clear_plane_b && (x < ext || x >= ext + width)) plane_b[x] = 0;
    }
}
