#ifndef TOPGEAR2_H
#define TOPGEAR2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOPGEAR2_ROM_SIZE          1048576u   /* 1 MB (8 Mbit) authentic ROM */
#define TOPGEAR2_WRAM_SIZE         131072u    /* 128 KB WRAM */
#define TOPGEAR2_FRAME_WIDTH_4_3   256
#define TOPGEAR2_FRAME_WIDTH_16_9  398
#define TOPGEAR2_WIDESCREEN_PAD    71
#define TOPGEAR2_FRAME_WIDTH       256
#define TOPGEAR2_FRAME_HEIGHT      224
#define TOPGEAR2_MAX_RENDER_SCALE  2
#define TOPGEAR2_MAX_FB_WIDTH      796
#define TOPGEAR2_MAX_FB_HEIGHT     448
#define TOPGEAR2_AUDIO_SAMPLE_RATE 32040

/* SNES Controller button masks */
#define TG2_KEY_B      0x8000u
#define TG2_KEY_Y      0x4000u
#define TG2_KEY_SELECT 0x2000u
#define TG2_KEY_START  0x1000u
#define TG2_KEY_UP     0x0800u
#define TG2_KEY_DOWN   0x0400u
#define TG2_KEY_LEFT   0x0200u
#define TG2_KEY_RIGHT  0x0100u
#define TG2_KEY_A      0x0080u
#define TG2_KEY_X      0x0040u
#define TG2_KEY_L      0x0020u
#define TG2_KEY_R      0x0010u

typedef enum Tg2RomStatus {
    TG2_ROM_OK = 0,
    TG2_ROM_ERR_IO = 1,
    TG2_ROM_ERR_SIZE = 2,
    TG2_ROM_ERR_HASH = 3,
    TG2_ROM_ERR_ALLOC = 4
} Tg2RomStatus;

typedef enum Tg2AspectMode {
    TG2_ASPECT_4_3 = 0,
    TG2_ASPECT_16_9 = 1
} Tg2AspectMode;

typedef enum Tg2VisualMode {
    TG2_VISUAL_PIXEL_PURO = 0,
    TG2_VISUAL_CRT_ARCADE = 1,
    TG2_VISUAL_ENHANCED_SMOOTH = 2,
    TG2_VISUAL_SYNTHWAVE = 3,
    TG2_VISUAL_ARCADE_VIVID = 4,
    TG2_VISUAL_VHS = 5,
    TG2_VISUAL_SUNSET = 6,
    TG2_VISUAL_COUNT = 7
} Tg2VisualMode;

typedef struct TopGear2Recomp TopGear2Recomp;

typedef struct TopGear2CpuState {
    uint16_t a, x, y, s, d, pc;
    uint8_t  p, e, pbr, dbr;
} TopGear2CpuState;

/* Lifecycle APIs */
TopGear2Recomp* tg2_create(void);
void tg2_destroy(TopGear2Recomp *instance);
bool tg2_load_rom(TopGear2Recomp *instance, const uint8_t *rom_data, size_t size);
bool tg2_load_rom_file(TopGear2Recomp *instance, const char *path);
Tg2RomStatus tg2_load_rom_file_checked(TopGear2Recomp *instance, const char *path, char *out_sha256_hex);
void tg2_reset(TopGear2Recomp *instance);

/* Frame advancement & execution */
int  tg2_step_frame(TopGear2Recomp *instance, uint16_t p1_input, uint16_t p2_input);
int  tg2_step_instruction(TopGear2Recomp *instance);

/* Display & Audio outputs */
const uint16_t* tg2_get_framebuffer_bgr555(const TopGear2Recomp *instance);
void tg2_get_framebuffer_rgba8888(const TopGear2Recomp *instance, uint32_t *out_rgba);
size_t tg2_get_audio_samples(TopGear2Recomp *instance, int16_t *out_pcm, size_t max_samples);

/* Definitive Edition: Display, Graphics & QoL APIs */
void tg2_set_aspect_mode(TopGear2Recomp *instance, Tg2AspectMode mode);
Tg2AspectMode tg2_get_aspect_mode(const TopGear2Recomp *instance);
void tg2_get_active_frame_size(const TopGear2Recomp *instance, int *out_w, int *out_h);

void tg2_set_visual_mode(TopGear2Recomp *instance, Tg2VisualMode mode);
Tg2VisualMode tg2_get_visual_mode(const TopGear2Recomp *instance);

void tg2_set_car_paint(TopGear2Recomp *instance, int player, int preset_id, uint8_t custom_r, uint8_t custom_g, uint8_t custom_b);
void tg2_get_car_paint(const TopGear2Recomp *instance, int player, int *out_preset, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b);

int tg2_render_hd_frame(TopGear2Recomp *instance, uint32_t *out_argb, int *out_w, int *out_h);
void tg2_rerender_current_frame(TopGear2Recomp *instance);

bool tg2_save_state_file(const TopGear2Recomp *instance, const char *path);
bool tg2_save_state_json_summary(const TopGear2Recomp *instance, const char *json_path);
bool tg2_load_state_file(TopGear2Recomp *instance, const char *path);

/* State query & modification */
void tg2_get_cpu_state(const TopGear2Recomp *instance, TopGear2CpuState *out_state);
void tg2_set_cpu_state(TopGear2Recomp *instance, const TopGear2CpuState *state);
bool tg2_jump_to_race_bank81(TopGear2Recomp *instance);
uint64_t tg2_get_frame_count(const TopGear2Recomp *instance);
uint64_t tg2_get_instruction_count(const TopGear2Recomp *instance);
const uint8_t* tg2_get_wram(const TopGear2Recomp *instance);
const uint8_t* tg2_get_vram(const TopGear2Recomp *instance);
const uint8_t* tg2_get_cgram(const TopGear2Recomp *instance);
const uint8_t* tg2_get_oam(const TopGear2Recomp *instance);
const uint8_t* tg2_get_ppu_registers(const TopGear2Recomp *instance);
uint8_t* tg2_get_wram_rw(TopGear2Recomp *instance);
const uint8_t* tg2_get_apu_ram(const TopGear2Recomp *instance);
uint16_t tg2_get_dsp_dir_page(const TopGear2Recomp *instance);

#ifdef __cplusplus
}
#endif

#endif /* TOPGEAR2_H */
