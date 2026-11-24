/*
 * topgear2_menu.c — Modern floating desktop configuration window for Top Gear 2: Definitive Edition
 *
 * Cross-platform (Windows + Linux), rendered into the logical ARGB8888 framebuffer.
 * Supports full mouse interaction (drag window, click tabs, toggles, steppers, sliders,
 * text input, action buttons), gamepad (D-Pad/L/R/A/B/Start), and keyboard navigation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "topgear2.h"
#include "topgear2_menu.h"
#include "topgear2_platform.h"

/* ═══════════════════════════════════════════════════════════════
   Modern Desktop / ImGui Dark Palette
   ═══════════════════════════════════════════════════════════════ */
#define COL_SCRIM        0x66060A12u
#define COL_SHADOW       0x90000000u
#define COL_WIN_BG       0xFF1A202Cu  /* Dark slate form background */
#define COL_WIN_SURFACE  0xFF222A3Au  /* Alt row / card surface */
#define COL_WIN_BORDER   0xFF334155u  /* Outer crisp border */
#define COL_TITLE_BG     0xFF243046u  /* Header bar background */
#define COL_ACCENT       0xFF3B82F6u  /* Modern cobalt blue accent */
#define COL_ACCENT_HOV   0xFF60A5FAu
#define COL_CLOSE_BG     0xFF991B1Bu
#define COL_CLOSE_HOV    0xFFEF4444u
#define COL_TAB_IDLE     0xFF232C3Du
#define COL_TAB_HOV      0xFF2E3A52u
#define COL_TAB_ACTIVE   0xFF2563EBu
#define COL_ROW_HOV      0xFF28344Au
#define COL_ROW_SEL      0xFF1E3A5Fu
#define COL_CTRL_BG      0xFF0F172Au
#define COL_CTRL_BORDER  0xFF475569u
#define COL_TOGGLE_ON    0xFF22C55Eu
#define COL_TOGGLE_OFF   0xFF475569u
#define COL_TEXT         0xFFF8FAFCu
#define COL_TEXT_SUB     0xFF94A3B8u
#define COL_TEXT_VAL     0xFF38BDF8u
#define COL_REBIND       0xFFF87171u
#define COL_WARN_BG      0xFF2A2115u
#define COL_WARN_BORDER  0xFFF59E0Bu
#define COL_WARN_TEXT    0xFFFDE68Au

/* ═══════════════════════════════════════════════════════════════
   Embedded 5x7 ASCII bitmap font (32..122)
   ═══════════════════════════════════════════════════════════════ */
static const uint8_t k_font[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}  /* z */
};

static const char* scancode_name(int sc, char *buf, size_t cap) {
    switch (sc) {
        case  4: return "A";     case  5: return "B";
        case  6: return "C";     case  7: return "D";
        case  8: return "E";     case  9: return "F";
        case 10: return "G";     case 11: return "H";
        case 12: return "I";     case 13: return "J";
        case 14: return "K";     case 15: return "L";
        case 16: return "M";     case 17: return "N";
        case 18: return "O";     case 19: return "P";
        case 20: return "Q";     case 21: return "R";
        case 22: return "S";     case 23: return "T";
        case 24: return "U";     case 25: return "V";
        case 26: return "W";     case 27: return "X";
        case 28: return "Y";     case 29: return "Z";
        case 30: return "1";     case 31: return "2";
        case 32: return "3";     case 33: return "4";
        case 34: return "5";     case 35: return "6";
        case 36: return "7";     case 37: return "8";
        case 38: return "9";     case 39: return "0";
        case 40: return "Enter"; case 41: return "Esc";
        case 42: return "BkSpc"; case 43: return "Tab";
        case 44: return "Space"; case 45: return "-";
        case 46: return "=";     case 79: return "Right";
        case 80: return "Left";  case 81: return "Down";
        case 82: return "Up";    case 229: return "RShift";
        case 225: return "LShift";
        case 224: return "LCtrl"; case 228: return "RCtrl";
        case 226: return "LAlt";  case 230: return "RAlt";
        default:
            snprintf(buf, cap, "SC%d", sc);
            return buf;
    }
}

static const char* const k_action_names[TG2_ACTION_COUNT] = {
    "Acelerar", "Freio / Re", "Nitro", "Marcha / Buzina",
    "Esquerda", "Direita", "Cima", "Baixo",
    "Start", "Select", "L (Ombro)", "R (Ombro)"
};

static const char* const k_tab_names[TG2_MENU_TAB_COUNT] = {
    "Controles", "Video", "Pintura", "Audio", "Jogo"
};

static const char* const k_tab_names_compact[TG2_MENU_TAB_COUNT] = {
    "Ctrl", "Video", "Cor", "Audio", "Jogo"
};

static const char* const k_visual_names[TG2_VISUAL_COUNT] = {
    "Pixel Puro",
    "CRT Trinitron",
    "Enhanced HD + Bloom",
    "Cyberpunk Synthwave",
    "Arcade 90s Vivid",
    "Retro VHS Composite",
    "Golden Sunset"
};

static const char* const k_paint_labels[8] = {
    "Original (ROM)", "Preto Onix", "Verde Neon", "Laranja McLaren",
    "Dourado Imperial", "Azul Metalico", "Rosa Cyberpunk", "Custom RGB"
};

static const uint32_t k_paint_swatch_col[8] = {
    0xFFD56241u, 0xFF30343Cu, 0xFF1EEB50u, 0xFFFF8000u,
    0xFFEBE028u, 0xFF1E78FFu, 0xFFFF2D91u, 0xFFFFFFFFu
};

static int items_in_tab(int tab) {
    switch (tab) {
        case TG2_MENU_TAB_CONTROLS: return TG2_ACTION_COUNT;
        case TG2_MENU_TAB_VIDEO:    return 4;
        case TG2_MENU_TAB_CARPAINT: return 8;
        case TG2_MENU_TAB_AUDIO:    return 3;
        case TG2_MENU_TAB_GAME:     return 7;
        default: return 0;
    }
}

/* ═══════════════════════════════════════════════════════════════
   Widget Geometry & Shared Hit-Testing Layout
   ═══════════════════════════════════════════════════════════════ */
typedef struct Tg2Rect {
    int x, y, w, h;
} Tg2Rect;

typedef struct Tg2MenuLayout {
    int fscale;
    Tg2Rect win;
    Tg2Rect title;
    Tg2Rect close_btn;
    Tg2Rect tabs[TG2_MENU_TAB_COUNT];
    Tg2Rect content;
    int row_h;
    int label_x;
    int ctrl_x;
    int ctrl_w;
    int footer_y;
} Tg2MenuLayout;

static bool pt_in_rect(int x, int y, Tg2Rect r) {
    return x >= r.x && x < (r.x + r.w) && y >= r.y && y < (r.y + r.h);
}

static void clamp_win_pos(Tg2MenuState *ms, int fb_w, int fb_h, int win_w, int win_h) {
    if (!ms->win_pos_init || ms->last_fb_w != fb_w || ms->last_fb_h != fb_h) {
        ms->win_x = (fb_w - win_w) / 2;
        ms->win_y = (fb_h - win_h) / 2;
        ms->last_fb_w = fb_w;
        ms->last_fb_h = fb_h;
        ms->win_pos_init = true;
    }
    if (ms->win_x < 2) ms->win_x = 2;
    if (ms->win_y < 2) ms->win_y = 2;
    if (ms->win_x + win_w > fb_w - 2) ms->win_x = fb_w - win_w - 2;
    if (ms->win_y + win_h > fb_h - 2) ms->win_y = fb_h - win_h - 2;
}

static void compute_layout(Tg2MenuState *ms, int fb_w, int fb_h, Tg2MenuLayout *lay) {
    int fs = (fb_w >= 512 && fb_h >= 400) ? 2 : 1;
    lay->fscale = fs;

    int win_w = (fs == 2) ? ((fb_w >= 700) ? 560 : 480) : ((fb_w >= 380) ? 352 : 244);
    int win_h = (fs == 2) ? 412 : 210;
    if (win_w > fb_w - 4) win_w = fb_w - 4;
    if (win_h > fb_h - 4) win_h = fb_h - 4;

    clamp_win_pos(ms, fb_w, fb_h, win_w, win_h);

    lay->win = (Tg2Rect){ ms->win_x, ms->win_y, win_w, win_h };
    int title_h = (fs == 2) ? 28 : 15;
    lay->title = (Tg2Rect){ lay->win.x, lay->win.y, win_w, title_h };

    int close_w = (fs == 2) ? 26 : 14;
    int close_h = (fs == 2) ? 18 : 11;
    lay->close_btn = (Tg2Rect){
        lay->win.x + win_w - close_w - (4 * fs),
        lay->win.y + (title_h - close_h) / 2,
        close_w, close_h
    };

    int tab_y = lay->win.y + title_h + (4 * fs);
    int tab_h = (fs == 2) ? 24 : 13;
    int pad_x = (fs == 2) ? 12 : 4;
    int gap = (fs == 2) ? 6 : 3;
    int avail_tab_w = win_w - pad_x * 2 - gap * (TG2_MENU_TAB_COUNT - 1);
    int each_tw = avail_tab_w / TG2_MENU_TAB_COUNT;
    for (int t = 0; t < TG2_MENU_TAB_COUNT; t++) {
        lay->tabs[t] = (Tg2Rect){ lay->win.x + pad_x + t * (each_tw + gap), tab_y, each_tw, tab_h };
    }

    int content_y = tab_y + tab_h + (5 * fs);
    int footer_h = (fs == 2) ? 24 : 13;
    lay->footer_y = lay->win.y + win_h - footer_h;
    lay->content = (Tg2Rect){
        lay->win.x + pad_x,
        content_y,
        win_w - pad_x * 2,
        lay->footer_y - content_y - (3 * fs)
    };
    lay->row_h = (fs == 2) ? 22 : 12;
    lay->label_x = lay->content.x + (6 * fs);
    lay->ctrl_x = lay->content.x + (lay->content.w * 43) / 100;
    lay->ctrl_w = lay->content.x + lay->content.w - lay->ctrl_x - (4 * fs);
}

static Tg2Rect row_rect(const Tg2MenuLayout *lay, int i) {
    return (Tg2Rect){ lay->content.x, lay->content.y + i * lay->row_h, lay->content.w, lay->row_h };
}

static bool row_is_slider(int tab, int item) {
    if (tab == TG2_MENU_TAB_CARPAINT && (item == 1 || item == 2 || item == 3 || item == 5 || item == 6 || item == 7))
        return true;
    if (tab == TG2_MENU_TAB_AUDIO && item == 0)
        return true;
    return false;
}

static bool row_is_toggle(int tab, int item) {
    if (tab == TG2_MENU_TAB_VIDEO && item == 2) return true;
    if (tab == TG2_MENU_TAB_AUDIO && item == 1) return true;
    return false;
}

static bool row_is_stepper(int tab, int item) {
    if (tab == TG2_MENU_TAB_VIDEO && (item == 0 || item == 1 || item == 3)) return true;
    if (tab == TG2_MENU_TAB_CARPAINT && (item == 0 || item == 4)) return true;
    if (tab == TG2_MENU_TAB_AUDIO && item == 2) return true;
    if (tab == TG2_MENU_TAB_GAME && item == 1) return true;
    return false;
}

static void get_slider_track_rect(const Tg2MenuLayout *lay, int row, Tg2Rect *out_track) {
    int fs = lay->fscale;
    int val_w = 24 * fs;
    int ry = lay->content.y + row * lay->row_h;
    int th = (fs == 2) ? 8 : 5;
    out_track->x = lay->ctrl_x + val_w + (4 * fs);
    out_track->y = ry + (lay->row_h - th) / 2;
    out_track->w = lay->ctrl_x + lay->ctrl_w - out_track->x - (4 * fs);
    out_track->h = th;
}

static void get_stepper_rects(const Tg2MenuLayout *lay, int row, Tg2Rect *out_prev, Tg2Rect *out_next, int reserve_swatch) {
    int fs = lay->fscale;
    int ry = lay->content.y + row * lay->row_h;
    int bw = (fs == 2) ? 20 : 11;
    int bh = lay->row_h - (2 * fs);
    int right_end = lay->ctrl_x + lay->ctrl_w - reserve_swatch;
    *out_prev = (Tg2Rect){ lay->ctrl_x, ry + fs, bw, bh };
    *out_next = (Tg2Rect){ right_end - bw, ry + fs, bw, bh };
}

static void get_toggle_rect(const Tg2MenuLayout *lay, int row, Tg2Rect *out_sw) {
    int fs = lay->fscale;
    int ry = lay->content.y + row * lay->row_h;
    int sw = (fs == 2) ? 44 : 24;
    int sh = (fs == 2) ? 16 : 9;
    *out_sw = (Tg2Rect){ lay->ctrl_x, ry + (lay->row_h - sh) / 2, sw, sh };
}

/* ═══════════════════════════════════════════════════════════════
   Drawing Primitives (ARGB8888)
   ═══════════════════════════════════════════════════════════════ */
static inline void put_px(uint32_t *buf, int w, int h, int x, int y, uint32_t col) {
    if (x >= 0 && x < w && y >= 0 && y < h)
        buf[y * w + x] = col;
}

static inline uint32_t alpha_blend(uint32_t dst, uint32_t src) {
    unsigned sa = (src >> 24) & 0xFFu;
    if (sa == 0xFFu) return src;
    if (sa == 0x00u) return dst;
    unsigned sr = (src >> 16) & 0xFFu, sg = (src >> 8) & 0xFFu, sb = src & 0xFFu;
    unsigned dr = (dst >> 16) & 0xFFu, dg = (dst >> 8) & 0xFFu, db = dst & 0xFFu;
    unsigned ia = 255u - sa;
    return 0xFF000000u |
        (((sr * sa + dr * ia) / 255u) << 16) |
        (((sg * sa + dg * ia) / 255u) << 8) |
        ((sb * sa + db * ia) / 255u);
}

static void fill_rect(uint32_t *buf, int bw, int bh, int x, int y, int rw, int rh, uint32_t col) {
    bool blend = ((col >> 24) & 0xFFu) < 0xFFu;
    for (int row = y; row < y + rh; row++) {
        if (row < 0 || row >= bh) continue;
        for (int col_x = x; col_x < x + rw; col_x++) {
            if (col_x < 0 || col_x >= bw) continue;
            buf[row * bw + col_x] = blend ? alpha_blend(buf[row * bw + col_x], col) : col;
        }
    }
}

static void draw_rect_border(uint32_t *buf, int bw, int bh, int x, int y, int rw, int rh, uint32_t col) {
    for (int i = 0; i < rw; i++) {
        put_px(buf, bw, bh, x + i, y, col);
        put_px(buf, bw, bh, x + i, y + rh - 1, col);
    }
    for (int i = 0; i < rh; i++) {
        put_px(buf, bw, bh, x, y + i, col);
        put_px(buf, bw, bh, x + rw - 1, y + i, col);
    }
}

static void draw_char(uint32_t *buf, int bw, int bh, int x, int y, char c, uint32_t color, int scale) {
    if (c < 32 || c > 122) c = ' ';
    const uint8_t *glyph = k_font[(int)(c - 32)];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if ((bits >> row) & 1) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        put_px(buf, bw, bh, x + col * scale + sx, y + row * scale + sy, color);
            }
        }
    }
}

static void draw_text(uint32_t *buf, int bw, int bh, int x, int y, const char *s, uint32_t color, int scale) {
    int cx = x;
    while (*s) {
        draw_char(buf, bw, bh, cx, y, *s, color, scale);
        cx += 6 * scale;
        s++;
    }
}

static int text_width(const char *s, int scale) {
    int len = (int)strlen(s);
    if (len <= 0) return 0;
    return len * 6 * scale - scale;
}

static void draw_text_centered(uint32_t *buf, int bw, int bh, Tg2Rect r, const char *s, uint32_t color, int scale) {
    int tw = text_width(s, scale);
    int th = 7 * scale;
    draw_text(buf, bw, bh, r.x + (r.w - tw) / 2, r.y + (r.h - th) / 2, s, color, scale);
}

static void draw_slider_widget(uint32_t *buf, int bw, int bh, Tg2Rect track, int val, int max_val, uint32_t fill_col, int fs) {
    fill_rect(buf, bw, bh, track.x, track.y, track.w, track.h, COL_CTRL_BG);
    draw_rect_border(buf, bw, bh, track.x, track.y, track.w, track.h, COL_CTRL_BORDER);
    int clamped = val < 0 ? 0 : (val > max_val ? max_val : val);
    int fw = (clamped * track.w) / (max_val > 0 ? max_val : 1);
    if (fw > 0) {
        fill_rect(buf, bw, bh, track.x + 1, track.y + 1, (fw > 2 ? fw - 2 : fw), track.h - 2, fill_col);
    }
    int knob_w = 4 * fs;
    int knob_h = track.h + 4 * fs;
    int kx = track.x + fw - knob_w / 2;
    if (kx < track.x) kx = track.x;
    if (kx + knob_w > track.x + track.w) kx = track.x + track.w - knob_w;
    int ky = track.y - 2 * fs;
    fill_rect(buf, bw, bh, kx, ky, knob_w, knob_h, COL_TEXT);
    draw_rect_border(buf, bw, bh, kx, ky, knob_w, knob_h, COL_WIN_BORDER);
}

static void draw_toggle_widget(uint32_t *buf, int bw, int bh, Tg2Rect sw, bool on, int fs) {
    uint32_t bg = on ? COL_TOGGLE_ON : COL_TOGGLE_OFF;
    fill_rect(buf, bw, bh, sw.x, sw.y, sw.w, sw.h, bg);
    draw_rect_border(buf, bw, bh, sw.x, sw.y, sw.w, sw.h, COL_WIN_BORDER);
    int thumb_w = sw.h - 2 * fs;
    if (thumb_w < 4) thumb_w = 4;
    int tx = on ? (sw.x + sw.w - thumb_w - fs) : (sw.x + fs);
    fill_rect(buf, bw, bh, tx, sw.y + fs, thumb_w, sw.h - 2 * fs, COL_TEXT);
    const char *lbl = on ? "ON" : "OFF";
    draw_text(buf, bw, bh, sw.x + sw.w + 5 * fs, sw.y + (sw.h - 7 * fs) / 2, lbl,
              on ? COL_TOGGLE_ON : COL_TEXT_SUB, fs);
}

static void draw_stepper_widget(uint32_t *buf, int bw, int bh, Tg2Rect prev, Tg2Rect next, const char *val_str, uint32_t val_col, int fs) {
    fill_rect(buf, bw, bh, prev.x, prev.y, prev.w, prev.h, COL_TAB_IDLE);
    draw_rect_border(buf, bw, bh, prev.x, prev.y, prev.w, prev.h, COL_CTRL_BORDER);
    draw_text_centered(buf, bw, bh, prev, "<", COL_TEXT, fs);

    Tg2Rect mid = { prev.x + prev.w + fs, prev.y, next.x - (prev.x + prev.w) - 2 * fs, prev.h };
    fill_rect(buf, bw, bh, mid.x, mid.y, mid.w, mid.h, COL_CTRL_BG);
    draw_rect_border(buf, bw, bh, mid.x, mid.y, mid.w, mid.h, COL_CTRL_BORDER);
    draw_text_centered(buf, bw, bh, mid, val_str, val_col, fs);

    fill_rect(buf, bw, bh, next.x, next.y, next.w, next.h, COL_TAB_IDLE);
    draw_rect_border(buf, bw, bh, next.x, next.y, next.w, next.h, COL_CTRL_BORDER);
    draw_text_centered(buf, bw, bh, next, ">", COL_TEXT, fs);
}

/* ═══════════════════════════════════════════════════════════════
   Shared Value Adjustment Helpers
   ═══════════════════════════════════════════════════════════════ */
static void finish_name_edit(Tg2MenuState *ms) {
    ms->editing_name = false;
    size_t len = strlen(ms->player_name);
    while (len > 0 && ms->player_name[len - 1] == ' ') {
        ms->player_name[--len] = '\0';
    }
    if (len == 0) {
        snprintf(ms->player_name, sizeof(ms->player_name), "PLAYER 1");
    }
    ms->pending_action = TG2_MENU_ACT_APPLY_GAME;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void adjust_item(Tg2MenuState *ms, int item, int delta, bool is_click) {
    if (ms->tab == TG2_MENU_TAB_CONTROLS) {
        if (is_click) ms->rebinding = item;
        return;
    }

    if (ms->tab == TG2_MENU_TAB_VIDEO) {
        switch (item) {
            case 0:
                ms->visual_mode = (ms->visual_mode + delta + TG2_VISUAL_COUNT) % TG2_VISUAL_COUNT;
                break;
            case 1:
                ms->aspect_mode = (ms->aspect_mode + 1) & 1;
                break;
            case 2:
                ms->fullscreen = !ms->fullscreen;
                break;
            case 3:
                if (is_click && delta == 1)
                    ms->window_scale = (ms->window_scale % 4) + 1;
                else
                    ms->window_scale = clamp_int(ms->window_scale + delta, 1, 4);
                break;
        }
        return;
    }

    if (ms->tab == TG2_MENU_TAB_CARPAINT) {
        int step = (delta > 0) ? 16 : -16;
        switch (item) {
            case 0: ms->p1_paint = (ms->p1_paint + delta + 8) % 8; break;
            case 1: case 2: case 3:
                ms->p1_paint = 7;
                ms->p1_rgb[item - 1] = (uint8_t)clamp_int((int)ms->p1_rgb[item - 1] + step, 0, 255);
                break;
            case 4: ms->p2_paint = (ms->p2_paint + delta + 8) % 8; break;
            case 5: case 6: case 7:
                ms->p2_paint = 7;
                ms->p2_rgb[item - 5] = (uint8_t)clamp_int((int)ms->p2_rgb[item - 5] + step, 0, 255);
                break;
        }
        return;
    }

    if (ms->tab == TG2_MENU_TAB_AUDIO) {
        switch (item) {
            case 0:
                ms->master_volume = clamp_int(ms->master_volume + delta * 5, 0, 100);
                break;
            case 1:
                ms->rumble_enabled = !ms->rumble_enabled;
                break;
            case 2:
                ms->rumble_intensity = (ms->rumble_intensity + delta + 3) % 3;
                break;
        }
        return;
    }

    if (ms->tab == TG2_MENU_TAB_GAME) {
        switch (item) {
            case 0:
                if (is_click) ms->editing_name = !ms->editing_name;
                break;
            case 1:
                ms->speed_unit = ms->speed_unit ? 0 : 1;
                ms->pending_action = TG2_MENU_ACT_APPLY_GAME;
                break;
            case 2:
                if (is_click) ms->pending_action = TG2_MENU_ACT_QUICK_SAVE;
                break;
            case 3:
                if (is_click) ms->pending_action = TG2_MENU_ACT_QUICK_LOAD;
                break;
            case 4:
                if (is_click) ms->pending_action = TG2_MENU_ACT_RESET_GAME;
                break;
            case 5:
                if (is_click) ms->pending_action = TG2_MENU_ACT_SELECT_ROM;
                break;
            case 6:
                if (is_click) ms->active = false;
                break;
        }
    }
}

static void apply_slider_drag(Tg2MenuState *ms, const Tg2MenuLayout *lay, int item, int mx) {
    Tg2Rect tr;
    get_slider_track_rect(lay, item, &tr);
    if (tr.w <= 1) return;
    int rel = clamp_int(mx - tr.x, 0, tr.w);

    if (ms->tab == TG2_MENU_TAB_CARPAINT) {
        uint8_t val = (uint8_t)((rel * 255 + tr.w / 2) / tr.w);
        if (item >= 1 && item <= 3) {
            ms->p1_paint = 7;
            ms->p1_rgb[item - 1] = val;
        } else if (item >= 5 && item <= 7) {
            ms->p2_paint = 7;
            ms->p2_rgb[item - 5] = val;
        }
    } else if (ms->tab == TG2_MENU_TAB_AUDIO && item == 0) {
        ms->master_volume = (rel * 100 + tr.w / 2) / tr.w;
    }
}

/* ═══════════════════════════════════════════════════════════════
   Public Lifecycle & Input Handlers
   ═══════════════════════════════════════════════════════════════ */
void tg2_menu_init(Tg2MenuState *ms) {
    memset(ms, 0, sizeof(*ms));

    ms->keys[TG2_ACTION_ACCEL]      = (Tg2KeyBind){ 6, 4};   /* C, A */
    ms->keys[TG2_ACTION_BRAKE]      = (Tg2KeyBind){29, 7};   /* Z, D */
    ms->keys[TG2_ACTION_NITRO]      = (Tg2KeyBind){27,-1};   /* X */
    ms->keys[TG2_ACTION_GEAR_HORN]  = (Tg2KeyBind){22,-1};   /* S */
    ms->keys[TG2_ACTION_LEFT]       = (Tg2KeyBind){80,-1};   /* Left */
    ms->keys[TG2_ACTION_RIGHT]      = (Tg2KeyBind){79,-1};   /* Right */
    ms->keys[TG2_ACTION_UP]         = (Tg2KeyBind){82,-1};   /* Up */
    ms->keys[TG2_ACTION_DOWN]       = (Tg2KeyBind){81,-1};   /* Down */
    ms->keys[TG2_ACTION_START]      = (Tg2KeyBind){40,-1};   /* Enter */
    ms->keys[TG2_ACTION_SELECT]     = (Tg2KeyBind){44,229};  /* Space, RShift */
    ms->keys[TG2_ACTION_L_SHOULDER] = (Tg2KeyBind){20,-1};   /* Q */
    ms->keys[TG2_ACTION_R_SHOULDER] = (Tg2KeyBind){26,-1};   /* W */

    ms->visual_mode  = TG2_VISUAL_ENHANCED_SMOOTH;
    ms->aspect_mode  = TG2_ASPECT_16_9;
    ms->fullscreen   = false;
    ms->window_scale = 2;

    ms->p1_paint = 0;
    ms->p2_paint = 0;
    ms->p1_rgb[0] = 255; ms->p1_rgb[1] = 128; ms->p1_rgb[2] = 0;
    ms->p2_rgb[0] = 30;  ms->p2_rgb[1] = 235; ms->p2_rgb[2] = 80;

    ms->master_volume    = 100;
    ms->rumble_enabled   = true;
    ms->rumble_intensity = 1;

    ms->speed_unit = 1;
    snprintf(ms->player_name, sizeof(ms->player_name), "PLAYER 1");
    ms->editing_name = false;

    ms->rebinding = -1;
    ms->hover_tab = -1;
    ms->hover_item = -1;
    ms->pending_action = TG2_MENU_ACT_NONE;
}

bool tg2_menu_is_active(const Tg2MenuState *ms) {
    return ms->active;
}

bool tg2_menu_handle_key(Tg2MenuState *ms, int scancode, bool is_down) {
    if (!is_down) return ms->active;

    if (scancode == 41) { /* Escape */
        if (ms->rebinding >= 0) {
            ms->rebinding = -1;
            return true;
        }
        if (ms->editing_name) {
            finish_name_edit(ms);
            return true;
        }
        ms->active = !ms->active;
        ms->dragging_win = false;
        ms->dragging_slider = false;
        if (ms->active) ms->blink_timer = 0;
        return true;
    }

    if (!ms->active) return false;

    if (ms->rebinding >= 0) {
        ms->keys[ms->rebinding].scancode = scancode;
        ms->rebinding = -1;
        return true;
    }

    if (ms->editing_name) {
        size_t len = strlen(ms->player_name);
        if (scancode == 40) { /* Enter */
            finish_name_edit(ms);
        } else if (scancode == 42) { /* Backspace */
            if (len > 0) ms->player_name[len - 1] = '\0';
        } else if (scancode == 76) { /* Delete */
            ms->player_name[0] = '\0';
        } else if (scancode >= 4 && scancode <= 29) {
            if (len < 11) {
                ms->player_name[len] = (char)('A' + (scancode - 4));
                ms->player_name[len + 1] = '\0';
            }
        } else if (scancode >= 30 && scancode <= 38) {
            if (len < 11) {
                ms->player_name[len] = (char)('1' + (scancode - 30));
                ms->player_name[len + 1] = '\0';
            }
        } else if (scancode == 39) {
            if (len < 11) {
                ms->player_name[len] = '0';
                ms->player_name[len + 1] = '\0';
            }
        } else if (scancode == 44) {
            if (len > 0 && len < 11) {
                ms->player_name[len] = ' ';
                ms->player_name[len + 1] = '\0';
            }
        }
        return true;
    }

    int max_items = items_in_tab(ms->tab);

    switch (scancode) {
        case 43: /* Tab -> next tab */
            ms->tab = (ms->tab + 1) % TG2_MENU_TAB_COUNT;
            ms->item = 0;
            return true;

        case 80: /* Left */
            if (row_is_slider(ms->tab, ms->item) || row_is_stepper(ms->tab, ms->item) || row_is_toggle(ms->tab, ms->item)) {
                adjust_item(ms, ms->item, -1, false);
            } else {
                ms->tab = (ms->tab + TG2_MENU_TAB_COUNT - 1) % TG2_MENU_TAB_COUNT;
                ms->item = 0;
            }
            return true;

        case 79: /* Right */
            if (row_is_slider(ms->tab, ms->item) || row_is_stepper(ms->tab, ms->item) || row_is_toggle(ms->tab, ms->item)) {
                adjust_item(ms, ms->item, 1, false);
            } else {
                ms->tab = (ms->tab + 1) % TG2_MENU_TAB_COUNT;
                ms->item = 0;
            }
            return true;

        case 82: /* Up */
            ms->item = (ms->item + max_items - 1) % max_items;
            return true;

        case 81: /* Down */
            ms->item = (ms->item + 1) % max_items;
            return true;

        case 40: /* Return */
            adjust_item(ms, ms->item, 1, true);
            return true;

        default:
            return true;
    }
}

bool tg2_menu_handle_pad(Tg2MenuState *ms, int pad_button, bool is_down) {
    if (!ms || !ms->active || !is_down) return ms ? ms->active : false;

    if (ms->rebinding >= 0) {
        if (pad_button == TG2_PAD_BTN_B || pad_button == TG2_PAD_BTN_BACK) {
            ms->rebinding = -1;
        }
        return true;
    }

    int max_items = items_in_tab(ms->tab);

    switch (pad_button) {
        case TG2_PAD_BTN_DPAD_UP:
            if (ms->editing_name) finish_name_edit(ms);
            ms->item = (ms->item + max_items - 1) % max_items;
            return true;

        case TG2_PAD_BTN_DPAD_DOWN:
            if (ms->editing_name) finish_name_edit(ms);
            ms->item = (ms->item + 1) % max_items;
            return true;

        case TG2_PAD_BTN_DPAD_LEFT:
            if (ms->editing_name) return true;
            if (row_is_slider(ms->tab, ms->item) || row_is_stepper(ms->tab, ms->item) || row_is_toggle(ms->tab, ms->item))
                adjust_item(ms, ms->item, -1, false);
            else {
                ms->tab = (ms->tab + TG2_MENU_TAB_COUNT - 1) % TG2_MENU_TAB_COUNT;
                ms->item = 0;
            }
            return true;

        case TG2_PAD_BTN_DPAD_RIGHT:
            if (ms->editing_name) return true;
            if (row_is_slider(ms->tab, ms->item) || row_is_stepper(ms->tab, ms->item) || row_is_toggle(ms->tab, ms->item))
                adjust_item(ms, ms->item, 1, false);
            else {
                ms->tab = (ms->tab + 1) % TG2_MENU_TAB_COUNT;
                ms->item = 0;
            }
            return true;

        case TG2_PAD_BTN_LEFTSHOULDER:
            if (ms->editing_name) finish_name_edit(ms);
            ms->tab = (ms->tab + TG2_MENU_TAB_COUNT - 1) % TG2_MENU_TAB_COUNT;
            ms->item = 0;
            return true;

        case TG2_PAD_BTN_RIGHTSHOULDER:
            if (ms->editing_name) finish_name_edit(ms);
            ms->tab = (ms->tab + 1) % TG2_MENU_TAB_COUNT;
            ms->item = 0;
            return true;

        case TG2_PAD_BTN_A:
            adjust_item(ms, ms->item, 1, true);
            return true;

        case TG2_PAD_BTN_B:
        case TG2_PAD_BTN_START:
            if (ms->editing_name) {
                finish_name_edit(ms);
                return true;
            }
            ms->active = false;
            ms->dragging_win = false;
            ms->dragging_slider = false;
            return true;

        default:
            return true;
    }
}

bool tg2_menu_handle_mouse(Tg2MenuState *ms, int mx, int my, int button, bool is_down, bool is_motion, int fb_w, int fb_h) {
    if (!ms || !ms->active) return false;
    if (fb_w <= 0 || fb_h <= 0) return false;

    Tg2MenuLayout lay;
    compute_layout(ms, fb_w, fb_h, &lay);

    if (is_motion) {
        if (ms->dragging_win) {
            ms->win_x = clamp_int(mx - ms->drag_off_x, 2, fb_w - lay.win.w - 2);
            ms->win_y = clamp_int(my - ms->drag_off_y, 2, fb_h - lay.win.h - 2);
            return true;
        }
        if (ms->dragging_slider) {
            apply_slider_drag(ms, &lay, ms->drag_slider_item, mx);
            return true;
        }

        ms->hover_close = pt_in_rect(mx, my, lay.close_btn);
        ms->hover_tab = -1;
        for (int t = 0; t < TG2_MENU_TAB_COUNT; t++) {
            if (pt_in_rect(mx, my, lay.tabs[t])) {
                ms->hover_tab = t;
                break;
            }
        }
        ms->hover_item = -1;
        int n = items_in_tab(ms->tab);
        for (int i = 0; i < n; i++) {
            if (pt_in_rect(mx, my, row_rect(&lay, i))) {
                ms->hover_item = i;
                break;
            }
        }
        return true;
    }

    /* Mouse Button Up */
    if (!is_down) {
        if (button == 1) {
            ms->dragging_win = false;
            ms->dragging_slider = false;
        }
        return true;
    }

    /* Only Left Mouse Button activates controls */
    if (button != 1) return true;

    if (ms->rebinding >= 0) {
        ms->rebinding = -1;
        return true;
    }

    /* 1. Close button [X] */
    if (pt_in_rect(mx, my, lay.close_btn)) {
        if (ms->editing_name) finish_name_edit(ms);
        ms->active = false;
        return true;
    }

    /* 2. Title bar drag */
    if (pt_in_rect(mx, my, lay.title)) {
        ms->dragging_win = true;
        ms->drag_off_x = mx - lay.win.x;
        ms->drag_off_y = my - lay.win.y;
        return true;
    }

    /* 3. Tab strip click */
    for (int t = 0; t < TG2_MENU_TAB_COUNT; t++) {
        if (pt_in_rect(mx, my, lay.tabs[t])) {
            if (ms->editing_name) finish_name_edit(ms);
            ms->tab = t;
            ms->item = 0;
            return true;
        }
    }

    /* 4. Content items */
    int n = items_in_tab(ms->tab);
    for (int i = 0; i < n; i++) {
        Tg2Rect rr = row_rect(&lay, i);
        if (!pt_in_rect(mx, my, rr)) continue;

        if (ms->editing_name && !(ms->tab == TG2_MENU_TAB_GAME && i == 0)) {
            finish_name_edit(ms);
        }

        ms->item = i;

        if (row_is_slider(ms->tab, i)) {
            Tg2Rect track;
            get_slider_track_rect(&lay, i, &track);
            if (mx >= track.x - 4 * lay.fscale && mx <= track.x + track.w + 4 * lay.fscale) {
                ms->dragging_slider = true;
                ms->drag_slider_item = i;
                apply_slider_drag(ms, &lay, i, mx);
            }
            return true;
        }

        if (row_is_stepper(ms->tab, i)) {
            Tg2Rect prev, next;
            int reserve = (ms->tab == TG2_MENU_TAB_CARPAINT && (i == 0 || i == 4)) ? (16 * lay.fscale) : 0;
            get_stepper_rects(&lay, i, &prev, &next, reserve);
            if (pt_in_rect(mx, my, prev)) {
                adjust_item(ms, i, -1, false);
                return true;
            }
            if (pt_in_rect(mx, my, next)) {
                adjust_item(ms, i, 1, false);
                return true;
            }
            adjust_item(ms, i, 1, true);
            return true;
        }

        adjust_item(ms, i, 1, true);
        return true;
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════════
   Modern Floating Window Rendering
   ═══════════════════════════════════════════════════════════════ */
void tg2_menu_render(Tg2MenuState *ms, uint32_t *argb, int w, int h) {
    if (!ms || !ms->active || !argb || w <= 0 || h <= 0) return;

    ms->blink_timer++;

    Tg2MenuLayout lay;
    compute_layout(ms, w, h, &lay);
    int fs = lay.fscale;

    /* 1. Soft background scrim so gameplay stays subtly visible */
    fill_rect(argb, w, h, 0, 0, w, h, COL_SCRIM);

    /* 2. Drop shadow behind floating window */
    fill_rect(argb, w, h, lay.win.x + 3 * fs, lay.win.y + 4 * fs, lay.win.w, lay.win.h, COL_SHADOW);

    /* 3. Main window frame & client surface */
    fill_rect(argb, w, h, lay.win.x, lay.win.y, lay.win.w, lay.win.h, COL_WIN_BG);
    draw_rect_border(argb, w, h, lay.win.x, lay.win.y, lay.win.w, lay.win.h, COL_WIN_BORDER);

    /* 4. Title bar + Cobalt Top Stripe + Window Icon + [X] Close Button */
    fill_rect(argb, w, h, lay.title.x + 1, lay.title.y + 1, lay.title.w - 2, lay.title.h - 1, COL_TITLE_BG);
    fill_rect(argb, w, h, lay.title.x + 1, lay.title.y + 1, lay.title.w - 2, fs, COL_ACCENT);

    int ico_s = (fs == 2) ? 10 : 5;
    int ico_x = lay.title.x + 6 * fs;
    int ico_y = lay.title.y + (lay.title.h - ico_s) / 2;
    fill_rect(argb, w, h, ico_x, ico_y, ico_s, ico_s, COL_ACCENT);
    fill_rect(argb, w, h, ico_x + 2, ico_y + 2, ico_s / 2, ico_s / 2, COL_TEXT);

    draw_text(argb, w, h, ico_x + ico_s + 5 * fs, lay.title.y + (lay.title.h - 7 * fs) / 2,
              "Top Gear 2 - Configuracoes", COL_TEXT, fs);

    fill_rect(argb, w, h, lay.close_btn.x, lay.close_btn.y, lay.close_btn.w, lay.close_btn.h,
              ms->hover_close ? COL_CLOSE_HOV : COL_CLOSE_BG);
    draw_rect_border(argb, w, h, lay.close_btn.x, lay.close_btn.y, lay.close_btn.w, lay.close_btn.h, COL_WIN_BORDER);
    draw_text_centered(argb, w, h, lay.close_btn, "X", COL_TEXT, fs);

    /* 5. Modern Tab Strip */
    for (int t = 0; t < TG2_MENU_TAB_COUNT; t++) {
        uint32_t bg = (t == ms->tab) ? COL_TAB_ACTIVE : ((t == ms->hover_tab) ? COL_TAB_HOV : COL_TAB_IDLE);
        uint32_t tc = (t == ms->tab) ? COL_TEXT : COL_TEXT_SUB;
        const char *tname = (fs == 1) ? k_tab_names_compact[t] : k_tab_names[t];
        fill_rect(argb, w, h, lay.tabs[t].x, lay.tabs[t].y, lay.tabs[t].w, lay.tabs[t].h, bg);
        draw_rect_border(argb, w, h, lay.tabs[t].x, lay.tabs[t].y, lay.tabs[t].w, lay.tabs[t].h,
                         (t == ms->tab) ? COL_ACCENT_HOV : COL_WIN_BORDER);
        draw_text_centered(argb, w, h, lay.tabs[t], tname, tc, fs);
    }

    /* Content border frame */
    fill_rect(argb, w, h, lay.content.x - fs, lay.content.y - fs,
              lay.content.w + 2 * fs, lay.content.h + 2 * fs, COL_WIN_SURFACE);
    draw_rect_border(argb, w, h, lay.content.x - fs, lay.content.y - fs,
                     lay.content.w + 2 * fs, lay.content.h + 2 * fs, COL_WIN_BORDER);

    /* 6. Tab Content Rows */
    int n = items_in_tab(ms->tab);
    for (int i = 0; i < n; i++) {
        Tg2Rect rr = row_rect(&lay, i);
        bool selected = (i == ms->item);
        bool hovered = (i == ms->hover_item);

        if (selected) {
            fill_rect(argb, w, h, rr.x, rr.y, rr.w, rr.h, COL_ROW_SEL);
            fill_rect(argb, w, h, rr.x, rr.y, 2 * fs, rr.h, COL_ACCENT);
        } else if (hovered) {
            fill_rect(argb, w, h, rr.x, rr.y, rr.w, rr.h, COL_ROW_HOV);
        } else if (i % 2 == 1) {
            fill_rect(argb, w, h, rr.x, rr.y, rr.w, rr.h, COL_WIN_BG);
        }

        int ty = rr.y + (rr.h - 7 * fs) / 2;

        if (ms->tab == TG2_MENU_TAB_CONTROLS) {
            draw_text(argb, w, h, lay.label_x, ty, k_action_names[i], COL_TEXT, fs);
            Tg2Rect box = { lay.ctrl_x, rr.y + fs, lay.ctrl_w, rr.h - 2 * fs };
            fill_rect(argb, w, h, box.x, box.y, box.w, box.h, COL_CTRL_BG);
            draw_rect_border(argb, w, h, box.x, box.y, box.w, box.h,
                             (ms->rebinding == i) ? COL_REBIND : COL_CTRL_BORDER);
            if (ms->rebinding == i) {
                draw_text_centered(argb, w, h, box, "PRESSIONE TECLA...", COL_REBIND, fs);
            } else {
                char bind_str[48];
                char s1[16], s2[16];
                if (ms->keys[i].alt_scancode >= 0)
                    snprintf(bind_str, sizeof(bind_str), "%s / %s",
                             scancode_name(ms->keys[i].scancode, s1, sizeof(s1)),
                             scancode_name(ms->keys[i].alt_scancode, s2, sizeof(s2)));
                else
                    snprintf(bind_str, sizeof(bind_str), "%s",
                             scancode_name(ms->keys[i].scancode, s1, sizeof(s1)));
                draw_text_centered(argb, w, h, box, bind_str, COL_TEXT_VAL, fs);
            }
        } else if (ms->tab == TG2_MENU_TAB_VIDEO) {
            const char *vlabels[4] = { "Filtro Visual", "Proporcao", "Tela Cheia", "Escala Janela" };
            draw_text(argb, w, h, lay.label_x, ty, vlabels[i], COL_TEXT, fs);

            if (i == 2) {
                Tg2Rect sw;
                get_toggle_rect(&lay, i, &sw);
                draw_toggle_widget(argb, w, h, sw, ms->fullscreen, fs);
            } else {
                Tg2Rect prev, next;
                get_stepper_rects(&lay, i, &prev, &next, 0);
                char vbuf[32];
                const char *vval = "";
                if (i == 0) vval = k_visual_names[(ms->visual_mode >= 0 && ms->visual_mode < TG2_VISUAL_COUNT) ? ms->visual_mode : 2];
                else if (i == 1) vval = ms->aspect_mode ? "16:9 Widescreen" : "4:3 Original";
                else { snprintf(vbuf, sizeof(vbuf), "%dx", ms->window_scale); vval = vbuf; }
                draw_stepper_widget(argb, w, h, prev, next, vval, COL_TEXT_VAL, fs);
            }
        } else if (ms->tab == TG2_MENU_TAB_CARPAINT) {
            const char *plabels[8] = {
                "P1 Pintura", "P1 Vermelho", "P1 Verde", "P1 Azul",
                "P2 Pintura", "P2 Vermelho", "P2 Verde", "P2 Azul"
            };
            draw_text(argb, w, h, lay.label_x, ty, plabels[i], COL_TEXT, fs);

            if (i == 0 || i == 4) {
                int preset = (i == 0) ? ms->p1_paint : ms->p2_paint;
                const uint8_t *rgb = (i == 0) ? ms->p1_rgb : ms->p2_rgb;
                int reserve = 16 * fs;
                Tg2Rect prev, next;
                get_stepper_rects(&lay, i, &prev, &next, reserve);
                draw_stepper_widget(argb, w, h, prev, next, k_paint_labels[preset & 7], COL_TEXT_VAL, fs);

                uint32_t sw_col = (preset == 7)
                    ? (0xFF000000u | ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | rgb[2])
                    : k_paint_swatch_col[preset & 7];
                int sx = lay.ctrl_x + lay.ctrl_w - 13 * fs;
                int sy = rr.y + 2 * fs;
                fill_rect(argb, w, h, sx, sy, 12 * fs, rr.h - 4 * fs, sw_col);
                draw_rect_border(argb, w, h, sx, sy, 12 * fs, rr.h - 4 * fs, COL_WIN_BORDER);
            } else {
                static const uint32_t chan_cols[3] = { 0xFFEF4444u, 0xFF22C55Eu, 0xFF3B82F6u };
                int ch_idx = (i <= 3) ? (i - 1) : (i - 5);
                int val = (i <= 3) ? ms->p1_rgb[ch_idx] : ms->p2_rgb[ch_idx];
                char cvbuf[8];
                snprintf(cvbuf, sizeof(cvbuf), "%3d", val);
                draw_text(argb, w, h, lay.ctrl_x, ty, cvbuf, COL_TEXT_VAL, fs);
                Tg2Rect tr;
                get_slider_track_rect(&lay, i, &tr);
                draw_slider_widget(argb, w, h, tr, val, 255, chan_cols[ch_idx], fs);
            }
        } else if (ms->tab == TG2_MENU_TAB_AUDIO) {
            const char *alabels[3] = { "Volume Geral", "Rumble Controle", "Intensidade Rumble" };
            draw_text(argb, w, h, lay.label_x, ty, alabels[i], COL_TEXT, fs);

            if (i == 0) {
                char vbuf[8];
                snprintf(vbuf, sizeof(vbuf), "%3d%%", ms->master_volume);
                draw_text(argb, w, h, lay.ctrl_x, ty, vbuf, COL_TEXT_VAL, fs);
                Tg2Rect tr;
                get_slider_track_rect(&lay, i, &tr);
                draw_slider_widget(argb, w, h, tr, ms->master_volume, 100, COL_ACCENT, fs);
            } else if (i == 1) {
                Tg2Rect sw;
                get_toggle_rect(&lay, i, &sw);
                draw_toggle_widget(argb, w, h, sw, ms->rumble_enabled, fs);
            } else {
                const char *rint[3] = { "Baixa", "Media", "Alta" };
                Tg2Rect prev, next;
                get_stepper_rects(&lay, i, &prev, &next, 0);
                draw_stepper_widget(argb, w, h, prev, next, rint[ms->rumble_intensity % 3], COL_TEXT_VAL, fs);
            }
        } else if (ms->tab == TG2_MENU_TAB_GAME) {
            const char *glabels[7] = {
                "Nome do Piloto", "Velocimetro", "Quick Save (F5)",
                "Quick Load (F8)", "Reiniciar Jogo", "Selecionar ROM",
                "Voltar a Corrida"
            };
            draw_text(argb, w, h, lay.label_x, ty, glabels[i], (i == 6) ? COL_ACCENT_HOV : COL_TEXT, fs);

            if (i == 0) {
                Tg2Rect box = { lay.ctrl_x, rr.y + fs, lay.ctrl_w, rr.h - 2 * fs };
                fill_rect(argb, w, h, box.x, box.y, box.w, box.h, COL_CTRL_BG);
                draw_rect_border(argb, w, h, box.x, box.y, box.w, box.h,
                                 ms->editing_name ? COL_ACCENT : COL_CTRL_BORDER);
                if (ms->editing_name) {
                    char nbuf[16];
                    bool cur = ((ms->blink_timer / 15) % 2) == 0;
                    snprintf(nbuf, sizeof(nbuf), "%s%s", ms->player_name, cur ? "_" : " ");
                    draw_text_centered(argb, w, h, box, nbuf, COL_TEXT, fs);
                } else {
                    draw_text_centered(argb, w, h, box, ms->player_name, COL_TEXT_VAL, fs);
                }
            } else if (i == 1) {
                Tg2Rect prev, next;
                get_stepper_rects(&lay, i, &prev, &next, 0);
                draw_stepper_widget(argb, w, h, prev, next, ms->speed_unit ? "KM/H" : "MPH", COL_TEXT_VAL, fs);
            } else {
                const char *btn_lbl[5] = { "Salvar Agora", "Carregar Agora", "Resetar ROM", "Procurar...", "Continuar" };
                Tg2Rect btn = { lay.ctrl_x, rr.y + fs, lay.ctrl_w, rr.h - 2 * fs };
                uint32_t bg = (i == 6) ? COL_TAB_ACTIVE : ((i == 4) ? COL_CLOSE_BG : COL_TAB_IDLE);
                fill_rect(argb, w, h, btn.x, btn.y, btn.w, btn.h, bg);
                draw_rect_border(argb, w, h, btn.x, btn.y, btn.w, btn.h,
                                 (selected || hovered) ? COL_ACCENT_HOV : COL_CTRL_BORDER);
                draw_text_centered(argb, w, h, btn, btn_lbl[i - 2], COL_TEXT, fs);
            }
        }
    }

    /* 7. Car Paint Disclaimer Box */
    if (ms->tab == TG2_MENU_TAB_CARPAINT) {
        int dy = lay.content.y + n * lay.row_h + 4 * fs;
        int dh = (lay.content.y + lay.content.h) - dy - 2 * fs;
        if (dh >= (fs == 2 ? 42 : 22)) {
            int dx = lay.content.x + 4 * fs;
            int dw = lay.content.w - 8 * fs;
            fill_rect(argb, w, h, dx, dy, dw, dh, COL_WARN_BG);
            draw_rect_border(argb, w, h, dx, dy, dw, dh, COL_WARN_BORDER);
            if (fs == 2) {
                draw_text(argb, w, h, dx + 8, dy + 6, "AVISO: Substitucao de Paleta em Tempo Real", COL_WARN_BORDER, 2);
                draw_text(argb, w, h, dx + 8, dy + 26, "A cor aplicada substitui a pintura na pista (incl. chuva).", COL_WARN_TEXT, 1);
                draw_text(argb, w, h, dx + 8, dy + 38, "Para usar a cor padrao escolhida na ROM, selecione Original.", COL_WARN_TEXT, 1);
            } else {
                draw_text(argb, w, h, dx + 4, dy + 3, "Nota: Muda paleta na pista em tempo real.", COL_WARN_BORDER, 1);
                draw_text(argb, w, h, dx + 4, dy + 13, "Use Original para manter a cor da ROM.", COL_WARN_TEXT, 1);
            }
        }
    }

    /* 8. Footer hint bar */
    const char *hint = ms->editing_name
        ? "[A-Z 0-9] Digitar  [Del] Limpar  [Enter] OK"
        : (fs == 2
            ? "[Mouse] Clicar/Arrastar  [Tab/LB/RB] Aba  [Esc/B] Fechar"
            : "[Mouse] Click/Drag  [Tab] Aba  [Esc] Fechar");
    int hscale = 1;
    int htw = text_width(hint, hscale);
    draw_text(argb, w, h, lay.win.x + (lay.win.w - htw) / 2,
              lay.footer_y + (lay.win.y + lay.win.h - lay.footer_y - 7 * hscale) / 2,
              hint, COL_TEXT_SUB, hscale);
}

static Tg2Rect rom_prompt_box_rect(int w, int h, int *out_fs) {
    int fs = (w >= 512 && h >= 400) ? 2 : 1;
    if (out_fs) *out_fs = fs;
    int bw = (fs == 2) ? 560 : 280;
    int bh = (fs == 2) ? 260 : 140;
    if (bw > w - 12) bw = w - 12;
    if (bh > h - 12) bh = h - 12;
    return (Tg2Rect){ (w - bw) / 2, (h - bh) / 2, bw, bh };
}

static Tg2Rect rom_prompt_btn_rect(int w, int h) {
    int fs = 1;
    Tg2Rect box = rom_prompt_box_rect(w, h, &fs);
    int btn_w = (fs == 2) ? 260 : 160;
    int btn_h = (fs == 2) ? 34 : 20;
    return (Tg2Rect){ box.x + (box.w - btn_w) / 2, box.y + box.h - btn_h - (18 * fs), btn_w, btn_h };
}

bool tg2_menu_rom_prompt_hit_button(int mx, int my, int w, int h) {
    return pt_in_rect(mx, my, rom_prompt_btn_rect(w, h));
}

void tg2_menu_render_rom_prompt(uint32_t *argb, int w, int h, const char *error_msg, bool btn_hover) {
    if (!argb || w <= 0 || h <= 0) return;

    for (int i = 0; i < w * h; i++) {
        argb[i] = 0xFF0B1120u;
    }

    int fs = 1;
    Tg2Rect box = rom_prompt_box_rect(w, h, &fs);
    Tg2Rect btn = rom_prompt_btn_rect(w, h);
    int title_h = (fs == 2) ? 30 : 16;

    fill_rect(argb, w, h, box.x + 3 * fs, box.y + 4 * fs, box.w, box.h, COL_SHADOW);
    fill_rect(argb, w, h, box.x, box.y, box.w, box.h, COL_WIN_BG);
    draw_rect_border(argb, w, h, box.x, box.y, box.w, box.h, COL_WIN_BORDER);

    fill_rect(argb, w, h, box.x + 1, box.y + 1, box.w - 2, title_h - 1, COL_TITLE_BG);
    fill_rect(argb, w, h, box.x + 1, box.y + 1, box.w - 2, fs, COL_ACCENT);
    draw_text_centered(argb, w, h, (Tg2Rect){ box.x, box.y, box.w, title_h },
                       "Top Gear 2 - Selecionar ROM", COL_TEXT, fs);

    int y0 = box.y + title_h + 14 * fs;
    int step_y = 14 * fs;
    draw_text_centered(argb, w, h, (Tg2Rect){ box.x, y0, box.w, 10 * fs },
                       "Selecione sua copia legal de Top Gear 2 (USA)", COL_TEXT, fs);
    draw_text_centered(argb, w, h, (Tg2Rect){ box.x, y0 + step_y, box.w, 10 * fs },
                       "Formato aceito: .sfc ou .smc (1 MB / LoROM)", COL_TEXT_SUB, fs);
    draw_text_centered(argb, w, h, (Tg2Rect){ box.x, y0 + step_y * 2, box.w, 10 * fs },
                       "Dica: clique abaixo ou arraste a ROM para esta janela", COL_TEXT_VAL, fs);

    if (error_msg && error_msg[0]) {
        Tg2Rect err_box = { box.x + 12 * fs, y0 + step_y * 3 + 2 * fs, box.w - 24 * fs, 18 * fs };
        fill_rect(argb, w, h, err_box.x, err_box.y, err_box.w, err_box.h, COL_WARN_BG);
        draw_rect_border(argb, w, h, err_box.x, err_box.y, err_box.w, err_box.h, COL_REBIND);
        char clipped[96];
        size_t max_chars = (size_t)((err_box.w > 8) ? (err_box.w - 8) / 6 : 1);
        if (max_chars > sizeof(clipped) - 1u) max_chars = sizeof(clipped) - 1u;
        snprintf(clipped, max_chars + 1u, "%s", error_msg);
        draw_text_centered(argb, w, h, err_box, clipped, COL_REBIND, 1);
    }

    fill_rect(argb, w, h, btn.x, btn.y, btn.w, btn.h, btn_hover ? COL_ACCENT : COL_TAB_ACTIVE);
    draw_rect_border(argb, w, h, btn.x, btn.y, btn.w, btn.h, COL_ACCENT_HOV);
    draw_text_centered(argb, w, h, btn, "Procurar ROM... (Enter)", COL_TEXT, fs);
}

void tg2_menu_render_pause_banner(uint32_t *argb, int w, int h) {
    if (!argb || w <= 0 || h <= 0) return;
    int fs = (w >= 512 && h >= 400) ? 2 : 1;
    fill_rect(argb, w, h, 0, 0, w, h, COL_SCRIM);
    int bw = (fs == 2) ? 220 : 120;
    int bh = (fs == 2) ? 46 : 26;
    Tg2Rect box = { (w - bw) / 2, (h - bh) / 2, bw, bh };
    fill_rect(argb, w, h, box.x + 2 * fs, box.y + 3 * fs, box.w, box.h, COL_SHADOW);
    fill_rect(argb, w, h, box.x, box.y, box.w, box.h, COL_WIN_BG);
    draw_rect_border(argb, w, h, box.x, box.y, box.w, box.h, COL_ACCENT);
    draw_text_centered(argb, w, h, box, "PAUSADO [P]", COL_TEXT, fs);
}

/* ═══════════════════════════════════════════════════════════════
   Key polling using current bindings
   ═══════════════════════════════════════════════════════════════ */
uint16_t tg2_menu_poll_keys(const Tg2MenuState *ms, const uint8_t *keyboard_state) {
    uint16_t input = 0;

    static const uint16_t action_to_snes[TG2_ACTION_COUNT] = {
        0x4000u, /* ACCEL     -> TG2_KEY_Y */
        0x8000u, /* BRAKE     -> TG2_KEY_B */
        0x0080u, /* NITRO     -> TG2_KEY_A */
        0x0040u, /* GEAR_HORN -> TG2_KEY_X */
        0x0200u, /* LEFT      -> TG2_KEY_LEFT */
        0x0100u, /* RIGHT     -> TG2_KEY_RIGHT */
        0x0800u, /* UP        -> TG2_KEY_UP */
        0x0400u, /* DOWN      -> TG2_KEY_DOWN */
        0x1000u, /* START     -> TG2_KEY_START */
        0x2000u, /* SELECT    -> TG2_KEY_SELECT */
        0x0020u, /* L_SHLD    -> TG2_KEY_L */
        0x0010u, /* R_SHLD    -> TG2_KEY_R */
    };

    for (int a = 0; a < TG2_ACTION_COUNT; a++) {
        int sc = ms->keys[a].scancode;
        int alt = ms->keys[a].alt_scancode;
        if (sc >= 0 && sc < 512 && keyboard_state[sc])
            input |= action_to_snes[a];
        if (alt >= 0 && alt < 512 && keyboard_state[alt])
            input |= action_to_snes[a];
    }

    return input;
}

/* ═══════════════════════════════════════════════════════════════
   Sync Player Name & Speed Unit into SNES WRAM
   ═══════════════════════════════════════════════════════════════ */
static void sanitize_player_name(const char *src, char out[12]) {
    memset(out, 0, 12);
    if (!src) {
        memcpy(out, "PLAYER 1", 8);
        return;
    }
    size_t j = 0;
    for (size_t i = 0; src[i] && j < 11; i++) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') {
            out[j++] = c;
        }
    }
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
    if (j == 0) {
        memcpy(out, "PLAYER 1", 8);
    }
}

void tg2_menu_apply_game_settings(const Tg2MenuState *ms, TopGear2Recomp *tg2) {
    if (!ms || !tg2) return;
    uint8_t *wram = tg2_get_wram_rw(tg2);
    if (!wram) return;

    char clean_name[12];
    sanitize_player_name(ms->player_name, clean_name);
    uint8_t unit_val = ms->speed_unit ? 1u : 0u;

    /* 1. Main Menu / Career struct ($00:1CD7..$00:1D70) */
    if (wram[0x1CD7] == 0x65u && wram[0x1CD8] == 0x7Eu &&
        wram[0x1CD9] == 0x43u && wram[0x1CDA] == 0x55u) {
        memcpy(&wram[0x1CF3], clean_name, 12);
        wram[0x1D1B] &= (uint8_t)~0x01u;
        wram[0x1CFF] = unit_val;
        wram[0x1D00] = 0u;
        wram[0x1D28] = unit_val;
        wram[0x1D29] = 0u;
    }

    /* 2. Pre-race Transfer Buffer ($7E:FE00..$7E:FE99) with checksum validation */
    uint16_t fe_sum = (uint16_t)(wram[0xFE00] | ((uint16_t)wram[0xFE01] << 8));
    uint16_t fe_inv = (uint16_t)(wram[0xFE02] | ((uint16_t)wram[0xFE03] << 8));
    if ((uint16_t)(fe_sum ^ fe_inv) == 0xFFFFu && (fe_sum != 0u || wram[0xFE1C] != 0u)) {
        uint16_t calc = 0u;
        for (int x = 4; x < 0x9A; x++) {
            calc = (uint16_t)(calc + wram[0xFE00 + x]);
        }
        if (calc == fe_sum) {
            memcpy(&wram[0xFE1C], clean_name, 12);
            wram[0xFE28] = unit_val;
            wram[0xFE29] = 0u;
            wram[0xFE51] = unit_val;
            wram[0xFE52] = 0u;

            uint16_t new_sum = 0u;
            for (int x = 4; x < 0x9A; x++) {
                new_sum = (uint16_t)(new_sum + wram[0xFE00 + x]);
            }
            wram[0xFE00] = (uint8_t)(new_sum & 0xFFu);
            wram[0xFE01] = (uint8_t)(new_sum >> 8);
            uint16_t new_inv = (uint16_t)(new_sum ^ 0xFFFFu);
            wram[0xFE02] = (uint8_t)(new_inv & 0xFFu);
            wram[0xFE03] = (uint8_t)(new_inv >> 8);
        }
    }

    /* 3. Active Race Engine (identified by BG1SC=0x78, BG2SC=0x71) */
    const uint8_t *ppu = tg2_get_ppu_registers(tg2);
    if (ppu && ppu[0x07] == 0x78u && ppu[0x08] == 0x71u) {
        uint8_t race_unit_byte = ms->speed_unit ? 0x00u : 0xFFu;
        wram[0x019A] = race_unit_byte;
        wram[0x019B] = race_unit_byte;
        wram[0x019C] = race_unit_byte;
        wram[0x019D] = race_unit_byte;

        memset(&wram[0x01E4], 0x20, 12);
        size_t nlen = strlen(clean_name);
        if (nlen > 12u) nlen = 12u;
        size_t pad_left = (12u - nlen) / 2u;
        memcpy(&wram[0x01E4 + pad_left], clean_name, nlen);

        static const uint8_t k_kmh_tiles[8] = { 0xB1u, 0x32u, 0xB2u, 0x32u, 0xB3u, 0x32u, 0xB0u, 0x32u };
        static const uint8_t k_mph_tiles[8] = { 0xA8u, 0x32u, 0xB2u, 0x32u, 0xB4u, 0x32u, 0xB0u, 0x32u };
        const uint8_t *target_tiles = ms->speed_unit ? k_kmh_tiles : k_mph_tiles;

        if (memcmp(&wram[0xCA9E], k_kmh_tiles, 8) == 0 || memcmp(&wram[0xCA9E], k_mph_tiles, 8) == 0) {
            memcpy(&wram[0xCA9E], target_tiles, 8);
        }
        if (memcmp(&wram[0xCA58], k_kmh_tiles, 8) == 0 || memcmp(&wram[0xCA58], k_mph_tiles, 8) == 0) {
            memcpy(&wram[0xCA58], target_tiles, 8);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
   Config persistence — minimal JSON read/write
   ═══════════════════════════════════════════════════════════════ */
static void json_write_escaped(FILE *f, const char *s) {
    if (!s) return;
    for (const char *p = s; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\\') fputs("\\\\", f);
        else if (ch == '"') fputs("\\\"", f);
        else if (ch == '\n') fputs("\\n", f);
        else if (ch == '\r') fputs("\\r", f);
        else if (ch == '\t') fputs("\\t", f);
        else if (ch >= 0x20u) fputc(ch, f);
    }
}

void tg2_menu_save_config(const Tg2MenuState *ms, const char *path) {
    FILE *f = tg2_fopen_utf8(path, "w");
    if (!f) return;

    fprintf(f, "{\n");

    fprintf(f, "  \"controls\": {\n");
    for (int i = 0; i < TG2_ACTION_COUNT; i++) {
        fprintf(f, "    \"k%d\": [%d, %d]%s\n", i,
                ms->keys[i].scancode, ms->keys[i].alt_scancode,
                (i < TG2_ACTION_COUNT - 1) ? "," : "");
    }
    fprintf(f, "  },\n");

    fprintf(f, "  \"video\": {\n");
    fprintf(f, "    \"visual_mode\": %d,\n", ms->visual_mode);
    fprintf(f, "    \"aspect_mode\": %d,\n", ms->aspect_mode);
    fprintf(f, "    \"fullscreen\": %s,\n", ms->fullscreen ? "true" : "false");
    fprintf(f, "    \"window_scale\": %d\n", ms->window_scale);
    fprintf(f, "  },\n");

    fprintf(f, "  \"car_paint\": {\n");
    fprintf(f, "    \"p1_preset\": %d,\n", ms->p1_paint);
    fprintf(f, "    \"p1_rgb\": [%d, %d, %d],\n", ms->p1_rgb[0], ms->p1_rgb[1], ms->p1_rgb[2]);
    fprintf(f, "    \"p2_preset\": %d,\n", ms->p2_paint);
    fprintf(f, "    \"p2_rgb\": [%d, %d, %d]\n", ms->p2_rgb[0], ms->p2_rgb[1], ms->p2_rgb[2]);
    fprintf(f, "  },\n");

    fprintf(f, "  \"audio\": {\n");
    fprintf(f, "    \"master_volume\": %d,\n", ms->master_volume);
    fprintf(f, "    \"rumble_enabled\": %s,\n", ms->rumble_enabled ? "true" : "false");
    fprintf(f, "    \"rumble_intensity\": %d\n", ms->rumble_intensity);
    fprintf(f, "  },\n");

    fprintf(f, "  \"game\": {\n");
    fprintf(f, "    \"speed_unit\": %d,\n", ms->speed_unit ? 1 : 0);
    fprintf(f, "    \"player_name\": \"");
    json_write_escaped(f, ms->player_name);
    fprintf(f, "\",\n");
    fprintf(f, "    \"rom_path\": \"");
    json_write_escaped(f, ms->rom_path);
    fprintf(f, "\"\n");
    fprintf(f, "  }\n");

    fprintf(f, "}\n");
    fclose(f);
}

static int jint(const char *line) {
    const char *p = strchr(line, ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

static bool jbool(const char *line) {
    return strstr(line, "true") != NULL;
}

static void jstr_raw(const char *line, char *out, size_t max_len) {
    if (!out || max_len == 0) return;
    out[0] = '\0';
    const char *p = strchr(line, ':');
    if (!p) return;
    const char *q1 = strchr(p + 1, '"');
    if (!q1) return;
    q1++;
    size_t i = 0;
    while (*q1 && *q1 != '"' && i + 1 < max_len) {
        char c = *q1++;
        if (c == '\\' && *q1) {
            char esc = *q1++;
            if (esc == 'n') c = '\n';
            else if (esc == 'r') c = '\r';
            else if (esc == 't') c = '\t';
            else c = esc;
        }
        out[i++] = c;
    }
    out[i] = '\0';
}

static void jpair(const char *line, int *a, int *b) {
    const char *br = strchr(line, '[');
    if (!br) { *a = 0; *b = -1; return; }
    *a = atoi(br + 1);
    const char *cm = strchr(br, ',');
    if (cm) *b = atoi(cm + 1);
    else *b = -1;
}

static void jrgb(const char *line, uint8_t *r, uint8_t *g, uint8_t *b_out) {
    int a = 0, bv = 0;
    const char *br = strchr(line, '[');
    if (!br) return;
    a = atoi(br + 1);
    *r = (uint8_t)(a & 0xFF);
    const char *c1 = strchr(br, ',');
    if (c1) { bv = atoi(c1 + 1); *g = (uint8_t)(bv & 0xFF); }
    const char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
    if (c2) { *b_out = (uint8_t)(atoi(c2 + 1) & 0xFF); }
}

bool tg2_menu_load_config(Tg2MenuState *ms, const char *path) {
    FILE *f = tg2_fopen_utf8(path, "r");
    if (!f) return false;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        for (int i = 0; i < TG2_ACTION_COUNT; i++) {
            char tag[16];
            snprintf(tag, sizeof(tag), "\"k%d\"", i);
            if (strstr(line, tag)) {
                int a, b;
                jpair(line, &a, &b);
                ms->keys[i].scancode = a;
                ms->keys[i].alt_scancode = b;
            }
        }

        if (strstr(line, "\"visual_mode\""))  ms->visual_mode  = jint(line);
        if (strstr(line, "\"aspect_mode\""))  ms->aspect_mode  = jint(line);
        if (strstr(line, "\"fullscreen\""))   ms->fullscreen   = jbool(line);
        if (strstr(line, "\"window_scale\"")) ms->window_scale = jint(line);

        if (strstr(line, "\"p1_preset\""))    ms->p1_paint = jint(line);
        if (strstr(line, "\"p2_preset\""))    ms->p2_paint = jint(line);
        if (strstr(line, "\"p1_rgb\""))       jrgb(line, &ms->p1_rgb[0], &ms->p1_rgb[1], &ms->p1_rgb[2]);
        if (strstr(line, "\"p2_rgb\""))       jrgb(line, &ms->p2_rgb[0], &ms->p2_rgb[1], &ms->p2_rgb[2]);

        if (strstr(line, "\"master_volume\""))    ms->master_volume    = jint(line);
        if (strstr(line, "\"rumble_enabled\""))   ms->rumble_enabled   = jbool(line);
        if (strstr(line, "\"rumble_intensity\"")) ms->rumble_intensity = jint(line);

        if (strstr(line, "\"speed_unit\""))       ms->speed_unit = jint(line);
        if (strstr(line, "\"player_name\"")) {
            char raw_name[32];
            jstr_raw(line, raw_name, sizeof(raw_name));
            sanitize_player_name(raw_name, ms->player_name);
        }
        if (strstr(line, "\"rom_path\"")) {
            jstr_raw(line, ms->rom_path, sizeof(ms->rom_path));
        }
    }

    fclose(f);

    if (ms->window_scale < 1) ms->window_scale = 1;
    if (ms->window_scale > 4) ms->window_scale = 4;
    if (ms->visual_mode < 0 || ms->visual_mode >= TG2_VISUAL_COUNT) ms->visual_mode = TG2_VISUAL_ENHANCED_SMOOTH;
    if (ms->aspect_mode < 0 || ms->aspect_mode > 1) ms->aspect_mode = 1;
    if (ms->master_volume < 0) ms->master_volume = 0;
    if (ms->master_volume > 100) ms->master_volume = 100;
    if (ms->p1_paint < 0 || ms->p1_paint > 7) ms->p1_paint = 0;
    if (ms->p2_paint < 0 || ms->p2_paint > 7) ms->p2_paint = 0;
    if (ms->rumble_intensity < 0 || ms->rumble_intensity > 2) ms->rumble_intensity = 1;
    ms->speed_unit = ms->speed_unit ? 1 : 0;
    if (ms->player_name[0] == '\0') snprintf(ms->player_name, sizeof(ms->player_name), "PLAYER 1");

    return true;
}
