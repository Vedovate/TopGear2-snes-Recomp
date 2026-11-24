#include "topgear2.h"
#include "topgear2_internal.h"
#include <string.h>
#include <stdlib.h>

typedef struct Tg2Pixel {
    uint16_t color;
    uint8_t palette_index, priority, layer, obj_palette, transparent;
} Tg2Pixel;

typedef struct Tg2Object {
    uint16_t x;
    uint8_t y, character, name_select, palette, priority, hflip, vflip, width, height, index;
} Tg2Object;

typedef struct Tg2ObjectLine {
    Tg2Object object;
    uint8_t line;
} Tg2ObjectLine;

typedef struct Tg2ObjectTile {
    uint16_t x;
    uint8_t priority, palette, hflip;
    uint32_t data;
} Tg2ObjectTile;

static inline uint8_t regv(const TopGear2Recomp *i, uint16_t a) {
    return i->ppu_registers[a - 0x2100u];
}

static inline uint16_t vr16(const TopGear2Recomp *i, uint32_t a) {
    return (uint16_t)(i->vram[a & 0xFFFFu] | ((uint16_t)i->vram[(a + 1u) & 0xFFFFu] << 8));
}

/* Preset definitions for Car Customization */
static const struct {
    uint8_t r, g, b;
} k_car_presets[8] = {
    {0, 0, 0},        /* 0: Original ROM */
    {48, 52, 60},     /* 1: Preto Onix */
    {30, 235, 80},    /* 2: Verde Neon */
    {255, 128, 0},    /* 3: Laranja McLaren */
    {235, 190, 40},   /* 4: Dourado Imperial */
    {30, 120, 255},   /* 5: Azul Metalico */
    {255, 45, 145},   /* 6: Rosa Cyberpunk */
    {255, 255, 255}   /* 7: Custom RGB */
};

static void tg2_compute_car_shades(int preset_id, const uint8_t custom_rgb[3], uint16_t bg1_out[5], uint16_t oam_out[4]) {
    uint8_t r, g, b;
    if (preset_id == 7 && custom_rgb) {
        r = custom_rgb[0];
        g = custom_rgb[1];
        b = custom_rgb[2];
    } else if (preset_id >= 1 && preset_id < 7) {
        r = k_car_presets[preset_id].r;
        g = k_car_presets[preset_id].g;
        b = k_car_presets[preset_id].b;
    } else {
        r = 213; g = 98; b = 65; /* Default authentic red */
    }

    unsigned r5 = r >> 3;
    unsigned g5 = g >> 3;
    unsigned b5 = b >> 3;

    /* BG1 Shade 0 (114): Highlight / Specular */
    unsigned r_hi = r5 + ((31u - r5) * 3u / 8u);
    unsigned g_hi = g5 + ((31u - g5) * 3u / 8u);
    unsigned b_hi = b5 + ((31u - b5) * 3u / 8u);
    if (r_hi > 31u) r_hi = 31u;
    if (g_hi > 31u) g_hi = 31u;
    if (b_hi > 31u) b_hi = 31u;
    bg1_out[0] = (uint16_t)(r_hi | (g_hi << 5) | (b_hi << 10));

    /* BG1 Shade 1 (115): Bright body */
    bg1_out[1] = (uint16_t)(r5 | (g5 << 5) | (b5 << 10));

    /* BG1 Shade 2 (116): Mid body */
    unsigned r_mid = r5 * 78u / 100u;
    unsigned g_mid = g5 * 78u / 100u;
    unsigned b_mid = b5 * 78u / 100u;
    bg1_out[2] = (uint16_t)(r_mid | (g_mid << 5) | (b_mid << 10));

    /* BG1 Shade 3 (117): Dark body */
    unsigned r_dark = r5 * 55u / 100u;
    unsigned g_dark = g5 * 55u / 100u;
    unsigned b_dark = b5 * 55u / 100u;
    bg1_out[3] = (uint16_t)(r_dark | (g_dark << 5) | (b_dark << 10));

    /* BG1 Shade 4 (118): Deep shadow */
    unsigned r_shd = r5 * 35u / 100u;
    unsigned g_shd = g5 * 35u / 100u;
    unsigned b_shd = b5 * 35u / 100u;
    bg1_out[4] = (uint16_t)(r_shd | (g_shd << 5) | (b_shd << 10));

    /* OAM Palette 0: index 133 is trim */
    unsigned r_trim = r5 * 85u / 100u;
    unsigned g_trim = g5 * 85u / 100u;
    unsigned b_trim = b5 * 85u / 100u;
    oam_out[0] = (uint16_t)(r_trim | (g_trim << 5) | (b_trim << 10));

    /* OAM 141, 142, 143 correspond to bright, mid, dark */
    oam_out[1] = bg1_out[1];
    oam_out[2] = bg1_out[2];
    oam_out[3] = bg1_out[3];
}

void tg2_update_car_paint_cache(TopGear2Recomp *instance) {
    if (!instance) return;
    uint16_t dummy_bg1[5];
    tg2_compute_car_shades(instance->car_paint_p1, instance->car_paint_rgb_p1,
                           instance->car_shades_p1_bg1, instance->car_shades_p1_oam);
    tg2_compute_car_shades(instance->car_paint_p2, instance->car_paint_rgb_p2,
                           dummy_bg1, instance->car_shades_p2_oam);
}

int tg2_is_two_player_race(const TopGear2Recomp *i) {
    if (!i || !tg2_is_active_race(i)) return 0;
    if ((i->nmitimen & 0x20u) != 0u && i->vtime_target >= 90u && i->vtime_target <= 130u) {
        return 1;
    }
    uint16_t fe_sum = (uint16_t)(i->wram[0xFE00] | ((uint16_t)i->wram[0xFE01] << 8));
    uint16_t fe_inv = (uint16_t)(i->wram[0xFE02] | ((uint16_t)i->wram[0xFE03] << 8));
    if ((uint16_t)(fe_sum ^ fe_inv) == 0xFFFFu && (fe_sum != 0u || i->wram[0xFE1C] != 0u)) {
        if (i->wram[0xFE04] != 0u) return 1;
    }
    return 0;
}

static inline uint16_t cg(const TopGear2Recomp *i, unsigned n) {
    uint32_t a = (n & 255u) * 2u;
    uint16_t c = (uint16_t)(i->cgram[a] | ((uint16_t)i->cgram[a + 1u] << 8)) & 0x7FFFu;

    if (i->car_paint_p1 > 0 &&
        ((n >= 114u && n <= 118u) || n == 133u || (n >= 141u && n <= 143u)) &&
        tg2_is_active_race(i)) {
        uint16_t c0 = (uint16_t)(i->cgram[0] | ((uint16_t)i->cgram[1] << 8)) & 0x7FFFu;
        uint16_t xor_mask = (c0 == 0x7FFFu && (regv(i, 0x2131u) & 0x80u) != 0 && (regv(i, 0x2130u) & 0x02u) != 0) ? 0x7FFFu : 0u;
        if (n >= 114u && n <= 118u) return (uint16_t)(i->car_shades_p1_bg1[n - 114u] ^ xor_mask);
        if (n == 133u) return (uint16_t)(i->car_shades_p1_oam[0] ^ xor_mask);
        return (uint16_t)(i->car_shades_p1_oam[n - 140u] ^ xor_mask);
    }

    if (i->car_paint_p2 > 0 && (n >= 237u && n <= 239u) && tg2_is_active_race(i)) {
        uint16_t c0 = (uint16_t)(i->cgram[0] | ((uint16_t)i->cgram[1] << 8)) & 0x7FFFu;
        uint16_t xor_mask = (c0 == 0x7FFFu && (regv(i, 0x2131u) & 0x80u) != 0 && (regv(i, 0x2130u) & 0x02u) != 0) ? 0x7FFFu : 0u;
        return (uint16_t)(i->car_shades_p2_oam[n - 236u] ^ xor_mask);
    }

    return c;
}

static inline uint16_t bright(uint16_t c, unsigned b) {
    unsigned r = c & 31u, g = (c >> 5) & 31u, bl = (c >> 10) & 31u;
    r = r * b / 15u;
    g = g * b / 15u;
    bl = bl * b / 15u;
    return (uint16_t)(r | (g << 5) | (bl << 10));
}

static inline uint16_t cadd(uint16_t a, uint16_t b, int half) {
    unsigned ar = a & 31u, ag = (a >> 5) & 31u, ab = (a >> 10) & 31u;
    unsigned br = b & 31u, bg = (b >> 5) & 31u, bb = (b >> 10) & 31u;
    unsigned r = ar + br, g = ag + bg, bl = ab + bb;
    if (half) { r >>= 1; g >>= 1; bl >>= 1; }
    else {
        if (r > 31) r = 31;
        if (g > 31) g = 31;
        if (bl > 31) bl = 31;
    }
    return (uint16_t)(r | (g << 5) | (bl << 10));
}

static inline uint16_t csub(uint16_t a, uint16_t b, int half) {
    int r = (int)(a & 31u) - (int)(b & 31u);
    int g = (int)((a >> 5) & 31u) - (int)((b >> 5) & 31u);
    int bl = (int)((a >> 10) & 31u) - (int)((b >> 10) & 31u);
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (bl < 0) bl = 0;
    if (half) { r >>= 1; g >>= 1; bl >>= 1; }
    return (uint16_t)(r | (g << 5) | (bl << 10));
}

static unsigned bpp(unsigned mode, unsigned l) {
    static const uint8_t t[7][4] = {
        {2,2,2,2}, {4,4,2,0}, {4,4,0,0}, {8,4,0,0}, {8,2,0,0}, {4,2,0,0}, {4,0,0,0}
    };
    return mode < 7 && l < 4 ? t[mode][l] : 0;
}

static uint8_t bgpri(unsigned mode, unsigned l, unsigned hi, int bg3hi) {
    static const uint8_t m0[4][2] = {{8,11},{7,10},{2,5},{1,4}};
    static const uint8_t m1[4][2] = {{6,9},{5,8},{1,3},{0,0}};
    static const uint8_t mx[4][2] = {{3,9},{1,7},{0,0},{0,0}};
    static const uint8_t m6[4][2] = {{3,9},{0,0},{0,0},{0,0}};
    if (mode == 0) return m0[l][hi != 0];
    if (mode == 1) {
        if (l == 2 && hi && bg3hi) return 11;
        return m1[l][hi != 0];
    }
    if (mode >= 2 && mode <= 5) return mx[l][hi != 0];
    if (mode == 6) return m6[l][hi != 0];
    return 0;
}

static uint8_t objpri(const TopGear2Recomp *i, unsigned mode, unsigned p) {
    static const uint8_t m0[4] = {3,6,9,12}, m1[4] = {2,4,7,10}, mx[4] = {2,4,6,8};
    static const uint8_t m7a[4] = {1,3,4,5}, m7b[4] = {2,4,6,7};
    p &= 3;
    if (mode == 0) return m0[p];
    if (mode == 1) return m1[p];
    if (mode == 7) return (regv(i, 0x2133u) & 0x40u) ? m7b[p] : m7a[p];
    return mx[p];
}

static uint16_t direct_color(uint8_t pal, uint8_t px) {
    unsigned r = ((px & 7u) << 2) | ((pal & 1u) << 1);
    unsigned g = ((px & 0x38u) >> 1) | ((pal & 2u) << 3);
    unsigned b = ((px & 0xC0u) >> 3) | ((pal & 4u) << 2);
    return (uint16_t)((r & 31u) | ((g & 31u) << 5) | ((b & 31u) << 10));
}

static int tiled_pixel(const TopGear2Recomp *i, unsigned l, unsigned x, unsigned y, Tg2Pixel *p, unsigned line_w, int is_race, int is_2p) {
    unsigned mode = regv(i, 0x2105u) & 7u;
    unsigned bb = bpp(mode, l);
    unsigned sample_x = x, sample_y = y;
    unsigned mosaic = ((unsigned)regv(i, 0x2106u) >> 4) + 1u;
    uint8_t sc, nba, pg, color = 0;
    unsigned ts, mw, mh, ww, wh, wx, wy, tx, ty, px, py, screen;
    uint32_t mapbase, charbase, mapaddr, row;
    uint16_t e, tile;
    unsigned bit, plane;

    memset(p, 0, sizeof(*p));
    p->transparent = 1;
    if (l >= 4 || !bb) return 1;

    sc = regv(i, (uint16_t)(0x2107u + l));
    mw = (sc & 1) ? 64 : 32;
    mh = (sc & 2) ? 64 : 32;
    mapbase = (uint32_t)(sc & 0xFCu) << 9;
    nba = regv(i, l < 2 ? 0x210Bu : 0x210Cu);
    charbase = (uint32_t)((l & 1) ? (nba >> 4) : (nba & 15u)) << 13;
    ts = (regv(i, 0x2105u) & (uint8_t)(0x10u << l)) ? 16 : 8;
    ww = mw * ts;
    wh = mh * ts;

    if (line_w > 256u) {
        if (is_race && !is_2p) {
            if (l == 0) {
                /* BG1 HUD & Player Car handling */
                if (y < 120u) {
                    /* Left HUD Cluster (RPM, Speed, Nitro, Damage, Fuel): anchor left with 12px margin */
                    if (x >= 12u && x <= 131u) {
                        sample_x = (x - 12u) + 8u;
                    }
                    /* Right HUD Cluster (Timer, Minimap, Lap, Pos): anchor right with 12px margin */
                    else if (x >= 266u && x <= 385u) {
                        sample_x = 128u + (x - 266u);
                    } else {
                        /* Gap between HUD clusters in 16:9 widescreen */
                        return 1;
                    }
                } else {
                    /* Starting banner & Player 1 Car body (centered on track, does not wrap) */
                    if (x >= 71u && x <= 326u) {
                        sample_x = x - 71u;
                    } else {
                        return 1;
                    }
                }
            } else {
                /* BG2 (Road/Horizon) & BG3 (Weather/Sky): wrap across 398 px centered at +71 */
                sample_x = (x + ww - 71u) % ww;
            }
        } else if (is_race && is_2p) {
            if (l == 0) {
                /* 2P Split-Screen BG1 viewport centered cleanly without splitting P1 top half */
                if (x >= 71u && x <= 326u) {
                    sample_x = x - 71u;
                } else {
                    return 1;
                }
            } else {
                sample_x = (x + ww - 71u) % ww;
            }
        } else {
            /* 2D Menus / Track selection centered on 16:9 */
            if (x >= 71u && x <= 326u) {
                sample_x = x - 71u;
            } else {
                return 1;
            }
        }
    }

    if ((regv(i, 0x2106u) & (1u << l)) && mosaic > 1) {
        sample_x -= sample_x % mosaic;
        sample_y -= sample_y % mosaic;
    }

    wx = (sample_x + i->bg_scroll[l * 2u]) % ww;
    wy = (sample_y + i->bg_scroll[l * 2u + 1u]) % wh;

    tx = wx / ts; ty = wy / ts; px = wx % ts; py = wy % ts;
    screen = (ty >> 5) * (mw == 64 ? 2u : 1u) + (tx >> 5);
    mapaddr = mapbase + screen * 0x800u + (((ty & 31u) * 32u + (tx & 31u)) * 2u);
    e = vr16(i, mapaddr);
    tile = (uint16_t)(e & 0x3FFu);
    pg = (uint8_t)((e >> 10) & 7u);

    /* In Widescreen, suppress 1px black overscan column border tile on BG1 */
    if (line_w > 256u && is_race && l == 0 && tile == 0x2A9u && (tx == 0u || tx == 31u)) {
        return 1;
    }

    if (e & 0x4000u) px = ts - 1u - px;
    if (e & 0x8000u) py = ts - 1u - py;
    if (ts == 16) {
        tile = (uint16_t)((tile + (px >> 3) + ((py >> 3) << 4)) & 0x3FFu);
        px &= 7; py &= 7;
    }

    row = charbase + (uint32_t)tile * (bb * 8u) + py * 2u;
    bit = 7u - px;
    for (plane = 0; plane < bb; plane++) {
        uint32_t a = row + (plane >> 1) * 16u + (plane & 1u);
        color |= (uint8_t)(((i->vram[a & 0xFFFFu] >> bit) & 1u) << plane);
    }
    if (!color) return 1;

    p->transparent = 0;
    p->priority = bgpri(mode, l, (e >> 13) & 1u, (regv(i, 0x2105u) & 8u) != 0);
    p->layer = (uint8_t)l;
    p->palette_index = color;

    if (bb == 2) {
        unsigned base = mode == 0 ? l * 32u : 0u;
        p->palette_index = (uint8_t)(base + pg * 4u + color);
        p->color = cg(i, p->palette_index);
    } else if (bb == 4) {
        p->palette_index = (uint8_t)(pg * 16u + color);
        p->color = cg(i, p->palette_index);
    } else {
        p->palette_index = color;
        p->color = (regv(i, 0x2130u) & 1u) ? direct_color(pg, color) : cg(i, color);
    }
    return 1;
}

static int32_t clip13(int32_t n) {
    return (n & 0x2000) ? (n | ~1023) : (n & 1023);
}

static int mode7_pixel(const TopGear2Recomp *i, unsigned l, unsigned x, unsigned y, Tg2Pixel *p, unsigned line_w) {
    int32_t a = (int16_t)i->mode7_a;
    int32_t b = (int16_t)i->mode7_b;
    int32_t c = (int16_t)i->mode7_c;
    int32_t d = (int16_t)i->mode7_d;
    int32_t hc = (int16_t)(i->mode7_x << 3) >> 3;
    int32_t vc = (int16_t)(i->mode7_y << 3) >> 3;
    int32_t ho = (int16_t)(i->mode7_hofs << 3) >> 3;
    int32_t vo = (int16_t)(i->mode7_vofs << 3) >> 3;
    int32_t xx = (int32_t)(line_w > 256u ? (x >= 71u && x <= 326u ? x - 71u : 0) : x);
    int32_t yy = (int32_t)y, ox, oy, px, py;
    uint16_t ta, pa;
    uint8_t tile, pal;
    unsigned repeat = (regv(i, 0x211Au) >> 6) & 3u;
    int oob;

    memset(p, 0, sizeof(*p));
    p->transparent = 1;
    if (line_w > 256u && (x < 71u || x > 326u)) return 1;
    if (l > 1) return 1;
    if (l == 1 && (regv(i, 0x2133u) & 0x40u) == 0) return 1;

    if (regv(i, 0x211Au) & 1u) xx = 255 - xx;
    if (regv(i, 0x211Au) & 2u) yy = 255 - yy;

    ox = (a * clip13(ho - hc) & ~63) + (b * clip13(vo - vc) & ~63) + (b * yy & ~63) + (hc << 8);
    oy = (c * clip13(ho - hc) & ~63) + (d * clip13(vo - vc) & ~63) + (d * yy & ~63) + (vc << 8);
    px = (ox + a * xx) >> 8;
    py = (oy + c * xx) >> 8;
    oob = ((px | py) & ~1023) != 0;

    ta = (uint16_t)((((uint32_t)py >> 3) & 0x7Fu) << 7 | (((uint32_t)px >> 3) & 0x7Fu));
    pa = (uint16_t)(((uint32_t)py & 7u) << 3 | ((uint32_t)px & 7u));
    tile = (repeat == 3 && oob) ? 0 : i->vram[((uint32_t)ta * 2u) & 0xFFFFu];
    pal = (repeat == 2 && oob) ? 0 : i->vram[(((uint32_t)((uint16_t)tile << 6 | pa) * 2u) + 1u) & 0xFFFFu];

    if (!pal) return 1;
    p->transparent = 0;
    p->layer = (uint8_t)l;
    p->priority = (l == 0) ? ((regv(i, 0x2133u) & 0x40u) ? 3u : 2u) : ((pal & 0x80u) ? 5u : 1u);
    if (l == 1) pal &= 0x7Fu;
    p->palette_index = pal;
    p->color = cg(i, pal);
    return 1;
}

static uint8_t sw(unsigned m) { static const uint8_t v[8]={8,8,8,16,16,32,16,16}; return v[m&7]; }
static uint8_t sh(unsigned m) { static const uint8_t v[8]={8,8,8,16,16,32,32,32}; return v[m&7]; }
static uint8_t lw(unsigned m) { static const uint8_t v[8]={16,32,64,32,64,64,32,32}; return v[m&7]; }
static uint8_t lh(unsigned m) { static const uint8_t v[8]={16,32,64,32,64,64,64,32}; return v[m&7]; }

static Tg2Object decobj(const TopGear2Recomp *i, unsigned n) {
    Tg2Object o;
    uint8_t hi = i->oam[512u + n / 4u];
    uint8_t pair = (uint8_t)((hi >> ((n & 3u) * 2u)) & 3u);
    uint8_t at = i->oam[n * 4u + 3u];
    uint8_t sel = regv(i, 0x2101u);
    uint8_t large = (pair >> 1) & 1u;
    uint8_t sm = sel >> 5;

    o.x = (uint16_t)(i->oam[n * 4u] | ((pair & 1u) << 8));
    o.y = (uint8_t)(i->oam[n * 4u + 1u] + 1u);
    o.character = i->oam[n * 4u + 2u];
    o.name_select = at & 1u;
    o.palette = (at >> 1) & 7u;
    o.priority = (at >> 4) & 3u;
    o.hflip = (at >> 6) & 1u;
    o.vflip = (at >> 7) & 1u;
    o.width = large ? lw(sm) : sw(sm);
    o.height = large ? lh(sm) : sh(sm);
    o.index = (uint8_t)n;
    return o;
}

int tg2_ws_try_capture_tile(TopGear2Recomp *instance, uint8_t size) {
    if (!instance || instance->aspect_mode != TG2_ASPECT_16_9 || !tg2_is_active_race(instance)) return 0;

    uint16_t d = instance->cpu.d;
    uint16_t tile_attr = 0u, raw_x = 0u, raw_y = 0u, raw_2e = 0u, raw_34 = 0u, raw_36 = 0u, raw_38 = 0u, raw_3a = 0u;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x1Au) & 0xFFFFu), &tile_attr)) return 0;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x1Cu) & 0xFFFFu), &raw_x)) return 0;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x1Eu) & 0xFFFFu), &raw_y)) return 0;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x2Eu) & 0xFFFFu), &raw_2e)) return 0;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x34u) & 0xFFFFu), &raw_34)) return 0;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x36u) & 0xFFFFu), &raw_36)) return 0;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x38u) & 0xFFFFu), &raw_38)) return 0;
    if (!tg2_bus_read16(instance, (uint32_t)((d + 0x3Au) & 0xFFFFu), &raw_3a)) return 0;

    int16_t tile_x = (int16_t)raw_x;
    int16_t inv_y  = (int16_t)raw_y;
    int16_t d2e    = (int16_t)raw_2e;
    int16_t d34    = (int16_t)raw_34;
    int16_t d36    = (int16_t)raw_36;
    int16_t d38    = (int16_t)raw_38;
    int16_t d3a    = (int16_t)raw_3a;

    if (d34 != 0 || d36 != 0x0100) return 0;
    if (tile_x + (int16_t)size > 0 && tile_x < 256) return 0;
    if (tile_x + (int16_t)size <= -71 || tile_x >= 327) return 1;
    if (inv_y - d38 >= (int16_t)(size - 1)) return 1;

    int16_t screen_y  = (int16_t)((d2e + 1) - inv_y);
    int16_t clip_ymin = (int16_t)((d2e + 1) - d38);
    int16_t clip_ymax = (int16_t)((d2e + 1) - d3a);
    if (screen_y >= clip_ymax || screen_y + (int16_t)size <= clip_ymin) return 1;

    if (instance->ws_staging_count < 256u) {
        Tg2WsSprite *s = &instance->ws_staging[instance->ws_staging_count++];
        s->x = (int16_t)(tile_x + 71);
        s->y = screen_y;
        s->clip_ymin = clip_ymin;
        s->clip_ymax = clip_ymax;
        s->tile_attr = tile_attr;
        s->size = size;
    }
    return 1;
}

static void obj_push_tiles(const TopGear2Recomp *core, uint8_t sel, int obj_x, unsigned yy,
                           uint8_t width, uint8_t ch, uint8_t name_select, uint8_t palette,
                           uint8_t priority, uint8_t hflip, unsigned line_w,
                           Tg2ObjectTile tiles[512], unsigned *tc) {
    unsigned base = (sel & 7u) << 13;
    unsigned cx = ch & 15u;
    unsigned cy = (((ch >> 4) + (yy >> 3)) & 15u) << 4;
    unsigned tw = width >> 3;
    if (name_select) base += (1u + ((sel >> 3) & 3u)) << 12;

    for (unsigned tx = 0; tx < tw; tx++) {
        if (*tc >= 512u) break;
        int sx = obj_x + (int)(tx << 3);
        unsigned ox = (unsigned)sx & 511u;
        if (sx >= (int)line_w && sx + 7 < 512) continue;
        unsigned mx = hflip ? tw - 1u - tx : tx;
        unsigned ad = base + ((cy + ((cx + mx) & 15u)) << 4);
        ad = (ad & 0xFFF0u) + (yy & 7u);
        uint16_t w0 = vr16(core, (ad * 2u) & 0xFFFFu);
        uint16_t w1 = vr16(core, ((ad + 8u) * 2u) & 0xFFFFu);
        Tg2ObjectTile *t = &tiles[(*tc)++];
        t->x = (uint16_t)ox;
        t->priority = priority;
        t->palette = (uint8_t)(128u + (palette << 4));
        t->hflip = hflip;
        t->data = (uint32_t)w0 | ((uint32_t)w1 << 16);
    }
}

static void objline(const TopGear2Recomp *core, unsigned y, uint8_t pals[TOPGEAR2_FRAME_WIDTH_16_9], uint8_t pris[TOPGEAR2_FRAME_WIDTH_16_9], unsigned line_w, int is_race, int is_2p) {
    Tg2ObjectLine items[256];
    Tg2ObjectTile tiles[512];
    unsigned ic = 0, tc = 0, scan, idx, t, p;
    uint8_t sel = regv(core, 0x2101u);
    unsigned first = core->oam_priority_rotation ? (((unsigned)core->oam_byte_address >> 1) & 127u) : 0u;

    memset(pals, 0, line_w);
    memset(pris, 0, line_w);

    for (scan = 0; scan < 128; scan++) {
        unsigned n = (first + scan) & 127u;
        Tg2Object o = decobj(core, n);
        uint8_t line = (uint8_t)((y + 1u - o.y) & 0xFFu);

        /* Skip hidden / off-screen SNES sentinel sprites before applying +71 widescreen shift */
        if ((o.x == 341u && o.y == 86u) || (o.x >= 256u && (uint32_t)o.x + o.width <= 512u)) {
            continue;
        }

        if (line_w > 256u) {
            if (is_race && !is_2p && n < 2u && y < 120u && o.y < 112u && o.x >= 128u && o.x < 256u) {
                /* Minimap cursor dots anchored with right HUD cluster */
                o.x = (uint16_t)(o.x + 138u);
            } else {
                o.x = (uint16_t)((o.x + 71u) & 511u);
            }
        }

        if ((o.x <= line_w || (uint32_t)o.x + o.width - 1u >= 512u) && line < o.height) {
            if (ic >= 256u) break;
            items[ic].object = o;
            items[ic].line = line;
            ic++;
        }
    }

    /* Render captured widescreen wing sprites first so nearer native OAM objects stay on top */
    if (line_w > 256u && is_race && core->ws_sprite_count > 0u) {
        int count = (int)core->ws_sprite_count;
        if (count > 256) count = 256;
        for (int wi = count - 1; wi >= 0 && tc < 512u; wi--) {
            const Tg2WsSprite *ws = &core->ws_sprites[wi];
            int sy = (int)y;
            if (sy < ws->clip_ymin || sy >= ws->clip_ymax) continue;
            if (sy < ws->y || sy >= ws->y + (int)ws->size) continue;

            uint8_t vflip = (uint8_t)((ws->tile_attr >> 15) & 1u);
            uint8_t hflip = (uint8_t)((ws->tile_attr >> 14) & 1u);
            uint8_t prio  = (uint8_t)((ws->tile_attr >> 12) & 3u);
            uint8_t pal   = (uint8_t)((ws->tile_attr >> 9) & 7u);
            uint8_t nsel  = (uint8_t)((ws->tile_attr >> 8) & 1u);
            uint8_t ch    = (uint8_t)(ws->tile_attr & 0xFFu);
            unsigned yy = (unsigned)(sy - ws->y);
            if (vflip) yy = ws->size - 1u - yy;

            obj_push_tiles(core, sel, ws->x, yy, ws->size, ch, nsel, pal, prio, hflip, line_w, tiles, &tc);
        }
    }

    for (idx = ic; idx > 0; idx--) {
        Tg2Object o = items[idx - 1].object;
        unsigned yy = items[idx - 1].line;
        unsigned xx = o.x & 511u;

        if (o.vflip) {
            if (o.width == o.height) yy = o.height - 1u - yy;
            else if (yy < o.width) yy = o.width - 1u - yy;
            else yy = o.width + (o.width - 1u) - (yy - o.width);
        }

        if (tc >= 512u) break;
        obj_push_tiles(core, sel, (int)xx, yy, o.width, o.character, o.name_select, o.palette, o.priority, o.hflip, line_w, tiles, &tc);
    }

    for (t = 0; t < tc; t++) {
        unsigned xx = tiles[t].x;
        for (p = 0; p < 8; p++) {
            xx &= 511u;
            if (xx < line_w) {
                unsigned shift = tiles[t].hflip ? p : (7u - p);
                uint8_t col = (uint8_t)(((tiles[t].data >> shift) & 1u) |
                                        (((tiles[t].data >> (shift + 8u)) & 1u) << 1) |
                                        (((tiles[t].data >> (shift + 16u)) & 1u) << 2) |
                                        (((tiles[t].data >> (shift + 24u)) & 1u) << 3));
                if (col) {
                    pals[xx] = (uint8_t)(tiles[t].palette + col);
                    pris[xx] = tiles[t].priority;
                }
            }
            xx++;
        }
    }
}

static uint8_t wnib(const TopGear2Recomp *i, unsigned l) {
    if (l < 2) return (uint8_t)((regv(i, 0x2123u) >> (l * 4u)) & 15u);
    if (l < 4) return (uint8_t)((regv(i, 0x2124u) >> ((l - 2u) * 4u)) & 15u);
    if (l == 4) return regv(i, 0x2125u) & 15u;
    return regv(i, 0x2125u) >> 4;
}

static unsigned wlogic(const TopGear2Recomp *i, unsigned l) {
    if (l < 4) return (regv(i, 0x212Au) >> (l * 2u)) & 3u;
    if (l == 4) return regv(i, 0x212Bu) & 3u;
    return (regv(i, 0x212Bu) >> 2) & 3u;
}

static inline int inside(unsigned x, unsigned a, unsigned b) {
    return a <= b && x >= a && x <= b;
}

static int wmask(const TopGear2Recomp *i, unsigned l, unsigned x, unsigned y, unsigned line_w, int is_race, int is_2p) {
    uint8_t n = wnib(i, l);
    int e1 = (n & 2) != 0, e2 = (n & 8) != 0;
    unsigned sx;
    int v1, v2;
    unsigned q;
    if (line_w > 256u) {
        if (is_race && !is_2p && y < 120u) {
            if (x >= 12u && x <= 131u) sx = (x - 12u) + 8u;
            else if (x >= 266u && x <= 385u) sx = 128u + (x - 266u);
            else return 0;
        } else {
            if (x >= 71u && x <= 326u) sx = x - 71u;
            else return 0; /* Wings outside SNES PPU window range: no clipping */
        }
    } else {
        sx = x;
    }
    v1 = inside(sx, regv(i, 0x2126u), regv(i, 0x2127u));
    v2 = inside(sx, regv(i, 0x2128u), regv(i, 0x2129u));
    if (!e1 && !e2) return 0;
    if (n & 1) v1 = !v1;
    if (n & 4) v2 = !v2;
    if (!e1) return v2;
    if (!e2) return v1;
    q = wlogic(i, l);
    if (q == 0) return v1 || v2;
    if (q == 1) return v1 && v2;
    if (q == 2) return v1 != v2;
    return v1 == v2;
}

static inline int applies(unsigned s, int in) {
    if (s == 0) return 0;
    if (s == 1) return !in;
    if (s == 2) return in;
    return 1;
}

static Tg2Pixel screenpx(const TopGear2Recomp *i, unsigned mask, unsigned wen, unsigned x, unsigned y, uint8_t opal, uint8_t opri, unsigned line_w, int is_race, int is_2p) {
    Tg2Pixel best;
    unsigned mode = regv(i, 0x2105u) & 7u, l;
    memset(&best, 0, sizeof(best));
    best.color = (!is_race && line_w > 256u && (x < 71u || x > 326u)) ? 0 : cg(i, 0);
    best.layer = 5;

    for (l = 0; l < 4; l++) {
        Tg2Pixel c;
        if ((mask & (1u << l)) == 0) continue;
        if ((wen & (1u << l)) && wmask(i, l, x, y, line_w, is_race, is_2p)) continue;
        if (mode == 7) {
            if (!mode7_pixel(i, l, x, y, &c, line_w)) continue;
        } else {
            if (!bpp(mode, l) || !tiled_pixel(i, l, x, y, &c, line_w, is_race, is_2p)) continue;
        }
        if (!c.transparent && c.priority > best.priority) best = c;
    }

    if ((mask & 0x10u) && opal) {
        uint8_t pr = objpri(i, mode, opri);
        if (!((wen & 0x10u) && wmask(i, 4, x, y, line_w, is_race, is_2p)) && pr > best.priority) {
            best.color = cg(i, opal);
            best.palette_index = opal;
            best.priority = pr;
            best.layer = 4;
            best.obj_palette = (uint8_t)((opal - 128u) >> 4);
        }
    }
    return best;
}

int tg2_render_scanline(TopGear2Recomp *i, uint16_t y) {
    unsigned x;
    uint8_t opal[TOPGEAR2_FRAME_WIDTH_16_9], opri[TOPGEAR2_FRAME_WIDTH_16_9];
    if (!i || y >= TOPGEAR2_FRAME_HEIGHT) return 0;

    /* Clear emissive mask at start of each frame to prevent bloom ghosting */
    if (y == 0) {
        memset(i->emissive_mask, 0, sizeof(i->emissive_mask));
    }

    int is_race = tg2_is_active_race(i);
    int is_2p = tg2_is_two_player_race(i);
    unsigned line_w = (i->aspect_mode == TG2_ASPECT_16_9) ? TOPGEAR2_FRAME_WIDTH_16_9 : TOPGEAR2_FRAME_WIDTH_4_3;

    /* Snapshot state for Sub-Scanline HD rendering */
    i->scanline_state[y].bg_mode = regv(i, 0x2105u);
    i->scanline_state[y].main_layers = regv(i, 0x212Cu) & 0x1Fu;
    i->scanline_state[y].sub_layers = regv(i, 0x212Du) & 0x1Fu;
    i->scanline_state[y].cg_math = regv(i, 0x2131u);
    i->scanline_state[y].cg_sub = regv(i, 0x2130u);
    i->scanline_state[y].brightness = regv(i, 0x2100u) & 15u;
    i->scanline_state[y].ppu_fixed_color = i->ppu_fixed_color;
    i->scanline_state[y].backdrop_color = cg(i, 0);
    i->scanline_state[y].win1_left = regv(i, 0x2126u);
    i->scanline_state[y].win1_right = regv(i, 0x2127u);
    i->scanline_state[y].is_race = (uint8_t)is_race;
    memcpy(i->scanline_state[y].bg_scroll, i->bg_scroll, sizeof(i->bg_scroll));

    objline(i, y, opal, opri, line_w, is_race, is_2p);

    for (x = 0; x < line_w; x++) {
        Tg2Pixel mp;
        Tg2Pixel sp;
        memset(&mp, 0, sizeof(mp));
        memset(&sp, 0, sizeof(sp));
        uint16_t color, add = i->ppu_fixed_color;
        uint8_t inid = regv(i, 0x2100u);
        uint8_t main = regv(i, 0x212Cu) & 0x1Fu;
        uint8_t sub = regv(i, 0x212Du) & 0x1Fu;
        uint8_t mw = regv(i, 0x212Eu) & 0x1Fu;
        uint8_t swm = regv(i, 0x212Fu) & 0x1Fu;
        uint8_t cgw = regv(i, 0x2130u);
        uint8_t cga = regv(i, 0x2131u);
        unsigned br = inid & 15u;
        int blank = (inid & 0x80u) != 0, cw, clip, prevent, math;

        if (blank) {
            color = 0;
        } else {
            mp = screenpx(i, main, mw, x, y, opal[x], opri[x], line_w, is_race, is_2p);
            color = mp.color;
            cw = wmask(i, 5, x, y, line_w, is_race, is_2p);
            clip = applies((cgw >> 6) & 3u, cw);
            prevent = applies((cgw >> 4) & 3u, cw);
            math = (cga & (1u << mp.layer)) != 0;

            int half = (cga & 0x40u) != 0;
            if (clip) color = 0;
            if (cgw & 2u) {
                sp = screenpx(i, sub, swm, x, y, opal[x], opri[x], line_w, is_race, is_2p);
                if (sp.layer == 5) {
                    add = i->ppu_fixed_color;
                    half = 0;
                } else {
                    add = sp.color;
                }
            }
            if (mp.layer == 4 && mp.obj_palette < 4) math = 0;
            if (prevent) math = 0;
            if (math) {
                color = (cga & 0x80u) ? csub(color, add, half)
                                      : cadd(color, add, half);
            }
            color = bright(color, br);
        }

        /* Check for emissive lights (taillights, nitro flames) for Bloom/Glow */
        uint8_t emissive = 0;
        if (is_race) {
            if (mp.layer == 4) {
                /* OAM P1 taillight (140), nitro flames (130, 131) */
                if (opal[x] == 140u || opal[x] == 130u || opal[x] == 131u) emissive = 1;
                /* Rival taillights & nitro */
                for (unsigned p = 1; p < 8; p++) {
                    uint8_t base = (uint8_t)(128u + p * 16u);
                    if (opal[x] == base + 12u || opal[x] == base + 2u || opal[x] == base + 3u) {
                        emissive = 1;
                        break;
                    }
                }
            } else if (mp.layer == 0) {
                /* BG1 P1 chassis taillights (125, 126) */
                if (mp.palette_index == 125u || mp.palette_index == 126u) emissive = 1;
            }
        }
        i->emissive_mask[(uint32_t)y * TOPGEAR2_FRAME_WIDTH_16_9 + x] = emissive;

        if (i->aspect_mode == TG2_ASPECT_16_9) {
            i->framebuffer_wide[(uint32_t)y * TOPGEAR2_FRAME_WIDTH_16_9 + x] = color;
            if (x >= 71u && x < 71u + 256u) {
                i->framebuffer[(uint32_t)y * TOPGEAR2_FRAME_WIDTH_4_3 + (x - 71u)] = color;
            }
        } else {
            i->framebuffer[(uint32_t)y * TOPGEAR2_FRAME_WIDTH_4_3 + x] = color;
        }
    }

    return 1;
}

static inline uint32_t bgr555_to_argb8888(uint16_t c) {
    uint32_t r = (uint32_t)((c & 0x001Fu) << 3) | ((c & 0x001Cu) >> 2);
    uint32_t g = (uint32_t)(((c >> 5) & 0x001Fu) << 3) | (((c >> 5) & 0x001Cu) >> 2);
    uint32_t b = (uint32_t)(((c >> 10) & 0x001Fu) << 3) | (((c >> 10) & 0x001Cu) >> 2);
    return (uint32_t)(0xFF000000u | (r << 16) | (g << 8) | b);
}

static inline uint32_t blend_argb(uint32_t c1, uint32_t c2, int weight_c2_16) {
    unsigned r1 = (c1 >> 16) & 0xFFu, g1 = (c1 >> 8) & 0xFFu, b1 = c1 & 0xFFu;
    unsigned r2 = (c2 >> 16) & 0xFFu, g2 = (c2 >> 8) & 0xFFu, b2 = c2 & 0xFFu;
    unsigned w2 = (unsigned)weight_c2_16;
    unsigned w1 = 16u - w2;
    unsigned r = (r1 * w1 + r2 * w2) >> 4;
    unsigned g = (g1 * w1 + g2 * w2) >> 4;
    unsigned b = (b1 * w1 + b2 * w2) >> 4;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline unsigned clamp_u8(int v) {
    if (v < 0) return 0u;
    if (v > 255) return 255u;
    return (unsigned)v;
}

static void render_hd_smooth_base(const TopGear2Recomp *instance, uint32_t *out_argb,
                                  const uint16_t *src_fb, int line_w, int hd_w, int is_race) {
    for (int y = 0; y < TOPGEAR2_FRAME_HEIGHT; y++) {
        int next_y = (y + 1 < TOPGEAR2_FRAME_HEIGHT) ? y + 1 : y;
        int row0 = (y * 2) * hd_w;
        int row1 = (y * 2 + 1) * hd_w;

        int is_road = (is_race && y >= 136 && y < 223);
        int road_shift_hd = 0;

        if (is_road) {
            int hofs0 = (int)instance->scanline_state[y].bg_scroll[2];
            int hofs1 = (int)instance->scanline_state[next_y].bg_scroll[2];
            int diff = hofs1 - hofs0;
            if (diff > -64 && diff < 64) {
                road_shift_hd = diff;
            }
        }

        for (int x = 0; x < line_w; x++) {
            uint32_t c_curr = bgr555_to_argb8888(src_fb[y * line_w + x]);
            uint32_t c_next = bgr555_to_argb8888(src_fb[next_y * line_w + x]);

            out_argb[row0 + x * 2]     = c_curr;
            out_argb[row0 + x * 2 + 1] = c_curr;

            if (is_road && road_shift_hd != 0) {
                int sx = x + (road_shift_hd > 0 ? 1 : -1);
                if (sx >= 0 && sx < line_w) {
                    uint32_t c_shift = bgr555_to_argb8888(src_fb[y * line_w + sx]);
                    out_argb[row1 + x * 2]     = blend_argb(c_curr, c_shift, 6);
                    out_argb[row1 + x * 2 + 1] = blend_argb(c_curr, c_next, 8);
                } else {
                    uint32_t c_mid = blend_argb(c_curr, c_next, 8);
                    out_argb[row1 + x * 2]     = c_mid;
                    out_argb[row1 + x * 2 + 1] = c_mid;
                }
            } else {
                uint32_t c_mid = blend_argb(c_curr, c_next, 8);
                out_argb[row1 + x * 2]     = c_mid;
                out_argb[row1 + x * 2 + 1] = c_mid;
            }
        }
    }
}

static void apply_emissive_bloom(const TopGear2Recomp *instance, uint32_t *out_argb,
                                 const uint16_t *src_fb, int line_w, int hd_w, int hd_h,
                                 unsigned gain_num, uint32_t tint_argb) {
    unsigned tr = (tint_argb >> 16) & 0xFFu;
    unsigned tg = (tint_argb >> 8) & 0xFFu;
    unsigned tb = tint_argb & 0xFFu;

    for (int y = 0; y < TOPGEAR2_FRAME_HEIGHT; y++) {
        for (int x = 0; x < line_w; x++) {
            if (!instance->emissive_mask[y * TOPGEAR2_FRAME_WIDTH_16_9 + x]) continue;
            uint32_t base_c = bgr555_to_argb8888(src_fb[y * line_w + x]);
            unsigned r_em = (((base_c >> 16) & 0xFFu) * tr) >> 8;
            unsigned g_em = (((base_c >> 8) & 0xFFu) * tg) >> 8;
            unsigned b_em = ((base_c & 0xFFu) * tb) >> 8;

            int cx = x * 2;
            int cy = y * 2;

            for (int dy = -2; dy <= 2; dy++) {
                int py = cy + dy;
                if (py < 0 || py >= hd_h) continue;
                for (int dx = -2; dx <= 2; dx++) {
                    int px = cx + dx;
                    if (px < 0 || px >= hd_w) continue;
                    int d_sq = dx * dx + dy * dy;
                    if (d_sq > 6) continue;
                    unsigned glow_fact = ((6u - (unsigned)d_sq) * 7u * gain_num) >> 4;
                    int p_idx = py * hd_w + px;
                    uint32_t dest = out_argb[p_idx];
                    unsigned dr = clamp_u8((int)((dest >> 16) & 0xFFu) + (int)((r_em * glow_fact) >> 8));
                    unsigned dg = clamp_u8((int)((dest >> 8) & 0xFFu) + (int)((g_em * glow_fact) >> 8));
                    unsigned db = clamp_u8((int)(dest & 0xFFu) + (int)((b_em * glow_fact) >> 8));
                    out_argb[p_idx] = 0xFF000000u | (dr << 16) | (dg << 8) | db;
                }
            }
        }
    }
}

int tg2_render_hd_frame(TopGear2Recomp *instance, uint32_t *out_argb, int *out_w, int *out_h) {
    if (!instance || !out_argb) return 0;

    int line_w = (instance->aspect_mode == TG2_ASPECT_16_9) ? TOPGEAR2_FRAME_WIDTH_16_9 : TOPGEAR2_FRAME_WIDTH_4_3;
    const uint16_t *src_fb = (instance->aspect_mode == TG2_ASPECT_16_9) ? instance->framebuffer_wide : instance->framebuffer;
    int is_race = tg2_is_active_race(instance);

    if (instance->visual_mode == TG2_VISUAL_PIXEL_PURO) {
        if (out_w) *out_w = line_w;
        if (out_h) *out_h = TOPGEAR2_FRAME_HEIGHT;

        for (int idx = 0; idx < line_w * TOPGEAR2_FRAME_HEIGHT; idx++) {
            out_argb[idx] = bgr555_to_argb8888(src_fb[idx]);
        }
        return 1;
    }

    int hd_w = line_w * 2;
    int hd_h = TOPGEAR2_FRAME_HEIGHT * 2;
    if (out_w) *out_w = hd_w;
    if (out_h) *out_h = hd_h;

    if (instance->visual_mode == TG2_VISUAL_CRT_ARCADE) {
        /* Trinitron CRT: deep 45% scanlines, RGB aperture grille phosphor mask, gamma punch + edge vignette */
        for (int y = 0; y < TOPGEAR2_FRAME_HEIGHT; y++) {
            int row0 = (y * 2) * hd_w;
            int row1 = (y * 2 + 1) * hd_w;
            int dy = y - (TOPGEAR2_FRAME_HEIGHT / 2);
            int y_vig = (dy * dy * 48) / (112 * 112);

            for (int x = 0; x < line_w; x++) {
                uint32_t argb = bgr555_to_argb8888(src_fb[y * line_w + x]);
                int dx = x - (line_w / 2);
                int vig = 256 - y_vig - (dx * dx * 48) / ((line_w / 2) * (line_w / 2));
                if (vig < 160) vig = 160;

                int r = (int)((argb >> 16) & 0xFFu);
                int g = (int)((argb >> 8) & 0xFFu);
                int b = (int)(argb & 0xFFu);

                /* Gamma & brightness compensation so scanline darkness keeps punch */
                r = (((r * 310) >> 8) * vig) >> 8;
                g = (((g * 310) >> 8) * vig) >> 8;
                b = (((b * 310) >> 8) * vig) >> 8;

                /* Even row: full beam + phosphor grille pair */
                out_argb[row0 + x * 2] = 0xFF000000u |
                    (clamp_u8((r * 276) >> 8) << 16) |
                    (clamp_u8((g * 224) >> 8) << 8) |
                    clamp_u8((b * 210) >> 8);
                out_argb[row0 + x * 2 + 1] = 0xFF000000u |
                    (clamp_u8((r * 212) >> 8) << 16) |
                    (clamp_u8((g * 272) >> 8) << 8) |
                    clamp_u8((b * 278) >> 8);

                /* Odd row: deep Trinitron scanline trench (~56% beam) */
                unsigned rd = clamp_u8((r * 145) >> 8);
                unsigned gd = clamp_u8((g * 145) >> 8);
                unsigned bd = clamp_u8((b * 145) >> 8);
                out_argb[row1 + x * 2]     = 0xFF000000u | (rd << 16) | (gd << 8) | bd;
                out_argb[row1 + x * 2 + 1] = 0xFF000000u | (rd << 16) | (gd << 8) | bd;
            }
        }
        return 1;
    }

    if (instance->visual_mode == TG2_VISUAL_ARCADE_VIVID) {
        /* 90s Super-Scaler Vivid: crisp upscale with +60% saturation and high-contrast S-curve */
        for (int y = 0; y < TOPGEAR2_FRAME_HEIGHT; y++) {
            int row0 = (y * 2) * hd_w;
            int row1 = (y * 2 + 1) * hd_w;
            for (int x = 0; x < line_w; x++) {
                uint32_t c = bgr555_to_argb8888(src_fb[y * line_w + x]);
                int r = (int)((c >> 16) & 0xFFu);
                int g = (int)((c >> 8) & 0xFFu);
                int b = (int)(c & 0xFFu);

                int luma = (r * 77 + g * 150 + b * 29) >> 8;
                /* +60% saturation punch around luma */
                r = luma + (((r - luma) * 410) >> 8);
                g = luma + (((g - luma) * 410) >> 8);
                b = luma + (((b - luma) * 410) >> 8);
                /* S-curve contrast */
                r = 128 + (((r - 128) * 312) >> 8);
                g = 128 + (((g - 128) * 312) >> 8);
                b = 128 + (((b - 128) * 312) >> 8);

                uint32_t outc = 0xFF000000u | (clamp_u8(r) << 16) | (clamp_u8(g) << 8) | clamp_u8(b);
                out_argb[row0 + x * 2]     = outc;
                out_argb[row0 + x * 2 + 1] = outc;
                out_argb[row1 + x * 2]     = outc;
                out_argb[row1 + x * 2 + 1] = outc;
            }
        }
        apply_emissive_bloom(instance, out_argb, src_fb, line_w, hd_w, hd_h, 14u, 0xFFFFFFFFu);
        return 1;
    }

    if (instance->visual_mode == TG2_VISUAL_VHS) {
        /* Retro VHS Composite: crisp luma + wide horizontal chroma bleed + analog black lift + tape lines */
        for (int y = 0; y < TOPGEAR2_FRAME_HEIGHT; y++) {
            int row0 = (y * 2) * hd_w;
            int row1 = (y * 2 + 1) * hd_w;
            int jitter = ((y ^ (int)instance->frame_count) & 3) == 0 ? 1 : 0;

            for (int x = 0; x < line_w; x++) {
                int lx = x + jitter;
                if (lx >= line_w) lx = line_w - 1;
                uint32_t cy_c = bgr555_to_argb8888(src_fb[y * line_w + lx]);
                int rl = (int)((cy_c >> 16) & 0xFFu);
                int gl = (int)((cy_c >> 8) & 0xFFu);
                int bl = (int)(cy_c & 0xFFu);
                int luma = ((rl * 77 + gl * 150 + bl * 29) >> 8) + 10;

                /* Wide composite chroma smear across 7 horizontal pixels */
                int sum_cr = 0, sum_cb = 0;
                for (int dx = -3; dx <= 3; dx++) {
                    int sx = x + dx;
                    if (sx < 0) sx = 0;
                    if (sx >= line_w) sx = line_w - 1;
                    uint32_t sc = bgr555_to_argb8888(src_fb[y * line_w + sx]);
                    int sr = (int)((sc >> 16) & 0xFFu);
                    int sg = (int)((sc >> 8) & 0xFFu);
                    int sb = (int)(sc & 0xFFu);
                    int sy_l = (sr * 77 + sg * 150 + sb * 29) >> 8;
                    sum_cr += (sr - sy_l);
                    sum_cb += (sb - sy_l);
                }
                int cr = (sum_cr / 7);
                int cb = (sum_cb / 7);
                int cg_off = -((cr * 130 + cb * 48) >> 8);

                unsigned or_ = clamp_u8(luma + cr + 6);
                unsigned og  = clamp_u8(luma + cg_off + 2);
                unsigned ob  = clamp_u8(luma + cb - 4);
                uint32_t col0 = 0xFF000000u | (or_ << 16) | (og << 8) | ob;

                unsigned rd = clamp_u8((int)or_ * 176 >> 8);
                unsigned gd = clamp_u8((int)og  * 176 >> 8);
                unsigned bd = clamp_u8((int)ob  * 176 >> 8);
                uint32_t col1 = 0xFF000000u | (rd << 16) | (gd << 8) | bd;

                out_argb[row0 + x * 2]     = col0;
                out_argb[row0 + x * 2 + 1] = col0;
                out_argb[row1 + x * 2]     = col1;
                out_argb[row1 + x * 2 + 1] = col1;
            }
        }
        return 1;
    }

    /* Sub-scanline HD base used by Enhanced HD, Synthwave, and Golden Sunset */
    render_hd_smooth_base(instance, out_argb, src_fb, line_w, hd_w, is_race);

    if (instance->visual_mode == TG2_VISUAL_SYNTHWAVE) {
        /* Cyberpunk Synthwave: neon magenta/violet/cyan color grade, retro scanlines + electric glow */
        for (int py = 0; py < hd_h; py++) {
            int row_off = py * hd_w;
            unsigned scan = (py & 1) ? 185u : 256u;
            for (int px = 0; px < hd_w; px++) {
                uint32_t c = out_argb[row_off + px];
                int r = (int)((c >> 16) & 0xFFu);
                int g = (int)((c >> 8) & 0xFFu);
                int b = (int)(c & 0xFFu);

                int nr = (((r * 305 + b * 40) >> 8) + 8);
                int ng = (((g * 190 + b * 36) >> 8));
                int nb = (((b * 320 + r * 52) >> 8) + 22);

                nr = (nr * (int)scan) >> 8;
                ng = (ng * (int)scan) >> 8;
                nb = (nb * (int)scan) >> 8;

                out_argb[row_off + px] = 0xFF000000u | (clamp_u8(nr) << 16) | (clamp_u8(ng) << 8) | clamp_u8(nb);
            }
        }
        apply_emissive_bloom(instance, out_argb, src_fb, line_w, hd_w, hd_h, 28u, 0xFFFF50FFu);
        return 1;
    }

    if (instance->visual_mode == TG2_VISUAL_SUNSET) {
        /* Golden Sunset: warm California dusk amber/crimson palette + golden tail glow */
        for (int py = 0; py < hd_h; py++) {
            int row_off = py * hd_w;
            unsigned horizon_warmth = (unsigned)(256 + ((hd_h - py) * 28) / hd_h);
            for (int px = 0; px < hd_w; px++) {
                uint32_t c = out_argb[row_off + px];
                int r = (int)((c >> 16) & 0xFFu);
                int g = (int)((c >> 8) & 0xFFu);
                int b = (int)(c & 0xFFu);

                int nr = (((r * (int)horizon_warmth) >> 8) * 310 >> 8) + 12;
                int ng = ((g * 240 + r * 20) >> 8) + 4;
                int nb = ((b * 145 + r * 12) >> 8);

                out_argb[row_off + px] = 0xFF000000u | (clamp_u8(nr) << 16) | (clamp_u8(ng) << 8) | clamp_u8(nb);
            }
        }
        apply_emissive_bloom(instance, out_argb, src_fb, line_w, hd_w, hd_h, 24u, 0xFFFFB040u);
        return 1;
    }

    /* Default: TG2_VISUAL_ENHANCED_SMOOTH — Sub-scanline smoothing + authentic taillight/nitro bloom */
    apply_emissive_bloom(instance, out_argb, src_fb, line_w, hd_w, hd_h, 16u, 0xFFFFFFFFu);
    return 1;
}

void tg2_rerender_current_frame(TopGear2Recomp *i) {
    if (!i) return;

    uint8_t saved_00 = i->ppu_registers[0x00u];
    uint8_t saved_05 = i->ppu_registers[0x05u];
    uint8_t saved_26 = i->ppu_registers[0x26u];
    uint8_t saved_27 = i->ppu_registers[0x27u];
    uint8_t saved_2c = i->ppu_registers[0x2Cu];
    uint8_t saved_2d = i->ppu_registers[0x2Du];
    uint8_t saved_30 = i->ppu_registers[0x30u];
    uint8_t saved_31 = i->ppu_registers[0x31u];
    uint16_t saved_fixed = i->ppu_fixed_color;
    uint16_t saved_scroll[8];
    memcpy(saved_scroll, i->bg_scroll, sizeof(saved_scroll));

    for (uint16_t y = 0u; y < TOPGEAR2_FRAME_HEIGHT; y++) {
        const Tg2ScanlineState st = i->scanline_state[y];
        i->ppu_registers[0x00u] = st.brightness & 0x0Fu;
        i->ppu_registers[0x05u] = st.bg_mode;
        i->ppu_registers[0x26u] = st.win1_left;
        i->ppu_registers[0x27u] = st.win1_right;
        i->ppu_registers[0x2Cu] = st.main_layers;
        i->ppu_registers[0x2Du] = st.sub_layers;
        i->ppu_registers[0x30u] = st.cg_sub;
        i->ppu_registers[0x31u] = st.cg_math;
        i->ppu_fixed_color = st.ppu_fixed_color;
        memcpy(i->bg_scroll, st.bg_scroll, sizeof(i->bg_scroll));
        tg2_render_scanline(i, y);
    }

    i->ppu_registers[0x00u] = saved_00;
    i->ppu_registers[0x05u] = saved_05;
    i->ppu_registers[0x26u] = saved_26;
    i->ppu_registers[0x27u] = saved_27;
    i->ppu_registers[0x2Cu] = saved_2c;
    i->ppu_registers[0x2Du] = saved_2d;
    i->ppu_registers[0x30u] = saved_30;
    i->ppu_registers[0x31u] = saved_31;
    i->ppu_fixed_color = saved_fixed;
    memcpy(i->bg_scroll, saved_scroll, sizeof(saved_scroll));
}
