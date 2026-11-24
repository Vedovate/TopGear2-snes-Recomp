#ifndef TOPGEAR2_MENU_H
#define TOPGEAR2_MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "topgear2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Tab indices ─────────────────────────────────────────────── */
#define TG2_MENU_TAB_CONTROLS  0
#define TG2_MENU_TAB_VIDEO     1
#define TG2_MENU_TAB_CARPAINT  2
#define TG2_MENU_TAB_AUDIO     3
#define TG2_MENU_TAB_GAME      4
#define TG2_MENU_TAB_COUNT     5

/* ── Gamepad menu button aliases (match SDL_CONTROLLER_BUTTON_*) ─ */
#define TG2_PAD_BTN_A             0
#define TG2_PAD_BTN_B             1
#define TG2_PAD_BTN_X             2
#define TG2_PAD_BTN_Y             3
#define TG2_PAD_BTN_BACK          4
#define TG2_PAD_BTN_START         6
#define TG2_PAD_BTN_LEFTSHOULDER  9
#define TG2_PAD_BTN_RIGHTSHOULDER 10
#define TG2_PAD_BTN_DPAD_UP       11
#define TG2_PAD_BTN_DPAD_DOWN     12
#define TG2_PAD_BTN_DPAD_LEFT     13
#define TG2_PAD_BTN_DPAD_RIGHT    14

/* ── Rebindable action indices ───────────────────────────────── */
#define TG2_ACTION_ACCEL       0
#define TG2_ACTION_BRAKE       1
#define TG2_ACTION_NITRO       2
#define TG2_ACTION_GEAR_HORN   3
#define TG2_ACTION_LEFT        4
#define TG2_ACTION_RIGHT       5
#define TG2_ACTION_UP          6
#define TG2_ACTION_DOWN        7
#define TG2_ACTION_START       8
#define TG2_ACTION_SELECT      9
#define TG2_ACTION_L_SHOULDER  10
#define TG2_ACTION_R_SHOULDER  11
#define TG2_ACTION_COUNT       12

/* ── Key binding pair (primary + alt) ────────────────────────── */
typedef struct Tg2KeyBind {
    int scancode;      /* SDL scancode (primary) */
    int alt_scancode;  /* SDL scancode (secondary), -1 = none */
} Tg2KeyBind;

/* ── Menu action notification returned to runner ─────────────── */
#define TG2_MENU_ACT_NONE        0
#define TG2_MENU_ACT_QUICK_SAVE  1
#define TG2_MENU_ACT_QUICK_LOAD  2
#define TG2_MENU_ACT_RESET_GAME  3
#define TG2_MENU_ACT_APPLY_GAME  4
#define TG2_MENU_ACT_SELECT_ROM  5

/* ── Full menu state ─────────────────────────────────────────── */
typedef struct Tg2MenuState {
    bool  active;         /* menu is visible and game is paused */
    int   tab;            /* current tab (0–4) */
    int   item;           /* selected item within current tab */
    int   rebinding;      /* -1 = normal, >=0 = waiting for key */
    int   blink_timer;    /* animation counter for cursor blink */
    int   pending_action; /* TG2_MENU_ACT_* triggered by click/pad/key */

    /* ── Floating window geometry & interaction state ── */
    int   win_x;
    int   win_y;
    int   last_fb_w;
    int   last_fb_h;
    bool  win_pos_init;
    bool  dragging_win;
    int   drag_off_x;
    int   drag_off_y;
    bool  dragging_slider;
    int   drag_slider_item;
    int   hover_tab;
    int   hover_item;
    bool  hover_close;

    /* ── Controls config ── */
    Tg2KeyBind keys[TG2_ACTION_COUNT];

    /* ── Video config ── */
    int   visual_mode;    /* 0..6 (Tg2VisualMode) */
    int   aspect_mode;    /* 0=4:3, 1=16:9 */
    bool  fullscreen;
    int   window_scale;   /* 1–4 */

    /* ── Car Paint config ── */
    int   p1_paint;       /* preset 0–7 */
    int   p2_paint;
    uint8_t p1_rgb[3];
    uint8_t p2_rgb[3];

    /* ── Audio config ── */
    int   master_volume;    /* 0–100 */
    bool  rumble_enabled;
    int   rumble_intensity; /* 0=Low, 1=Medium, 2=High */

    /* ── Game / Player config ── */
    int   speed_unit;       /* 0=MPH, 1=KM/H */
    char  player_name[12];  /* Up to 11 uppercase chars + NUL */
    bool  editing_name;     /* true while typing player name in menu */
    char  rom_path[512];    /* Last verified ROM file path */
} Tg2MenuState;

/* ── Public API ──────────────────────────────────────────────── */

/* Initialize menu state with defaults. */
void tg2_menu_init(Tg2MenuState *ms);

/* Handle a keyboard event. Returns true if the menu consumed it. */
bool tg2_menu_handle_key(Tg2MenuState *ms, int scancode, bool is_down);

/* Handle a mouse event in logical framebuffer coordinates. Returns true if consumed. */
bool tg2_menu_handle_mouse(Tg2MenuState *ms, int mx, int my, int button, bool is_down, bool is_motion, int fb_w, int fb_h);

/* Handle a gamepad controller button press/release in the menu. Returns true if consumed. */
bool tg2_menu_handle_pad(Tg2MenuState *ms, int pad_button, bool is_down);

/* Render the floating menu window onto an ARGB8888 framebuffer. */
void tg2_menu_render(Tg2MenuState *ms, uint32_t *argb, int w, int h);

/* Render welcome/error prompt to select or drag-and-drop a ROM file. */
void tg2_menu_render_rom_prompt(uint32_t *argb, int w, int h, const char *error_msg, bool btn_hover);
bool tg2_menu_rom_prompt_hit_button(int mx, int my, int w, int h);

/* Render centered pause banner overlay on ARGB8888 framebuffer. */
void tg2_menu_render_pause_banner(uint32_t *argb, int w, int h);

/* Returns true if the menu is currently active (game should pause). */
bool tg2_menu_is_active(const Tg2MenuState *ms);

/* Save config to a JSON file. */
void tg2_menu_save_config(const Tg2MenuState *ms, const char *path);

/* Load config from a JSON file. Returns false if file not found. */
bool tg2_menu_load_config(Tg2MenuState *ms, const char *path);

/* Map current key bindings to SNES controller buttons for input polling. */
uint16_t tg2_menu_poll_keys(const Tg2MenuState *ms, const uint8_t *keyboard_state);

/* Sync Player Name and Speed Unit (KM/H vs MPH) into SNES WRAM. */
void tg2_menu_apply_game_settings(const Tg2MenuState *ms, TopGear2Recomp *tg2);

#ifdef __cplusplus
}
#endif

#endif /* TOPGEAR2_MENU_H */
