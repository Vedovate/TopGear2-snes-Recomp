#ifndef TOPGEAR2_INTERNAL_H
#define TOPGEAR2_INTERNAL_H

#include "topgear2.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define TG2_P_C 0x01u
#define TG2_P_Z 0x02u
#define TG2_P_I 0x04u
#define TG2_P_D 0x08u
#define TG2_P_X 0x10u
#define TG2_P_M 0x20u
#define TG2_P_V 0x40u
#define TG2_P_N 0x80u

typedef struct TopGear2CpuInternal {
    uint16_t a;
    uint16_t x;
    uint16_t y;
    uint16_t s;
    uint16_t d;
    uint16_t pc;
    uint8_t  p;
    uint8_t  e;
    uint8_t  pbr;
    uint8_t  dbr;
} TopGear2CpuInternal;

/* Forward declaration of external subsystems */
struct Apu;
struct Interp816;

typedef struct Tg2ScanlineState {
    uint16_t bg_scroll[8];
    uint8_t  bg_mode;
    uint8_t  main_layers;
    uint8_t  sub_layers;
    uint8_t  cg_math;
    uint8_t  cg_sub;
    uint8_t  brightness;
    uint16_t ppu_fixed_color;
    uint16_t backdrop_color;
    uint8_t  win1_left, win1_right;
    uint8_t  is_race;
} Tg2ScanlineState;

typedef struct Tg2WsSprite {
    int16_t  x;
    int16_t  y;
    int16_t  clip_ymin;
    int16_t  clip_ymax;
    uint16_t tile_attr;
    uint8_t  size;
} Tg2WsSprite;

struct TopGear2Recomp {
    uint8_t *rom;
    size_t   rom_size;
    TopGear2CpuInternal cpu;

    uint8_t wram[TOPGEAR2_WRAM_SIZE];

    /* SNES PPU registers and video memory */
    uint8_t ppu_registers[0x100];
    uint8_t vram[65536];
    uint8_t oam[544];
    uint8_t cgram[512];

    uint16_t vram_word_address;
    uint16_t oam_byte_address;
    uint16_t cgram_byte_address;
    uint8_t  vram_increment_words;
    uint8_t  vram_increment_after_high;
    uint8_t  cgram_latch_low;
    uint8_t  cgram_latch_pending;
    uint8_t  oam_latch_low;
    uint8_t  oam_latch_pending;
    uint8_t  vram_mapping;
    uint16_t vram_read_latch;
    uint32_t wram_port_address;
    uint8_t  forced_blank;
    uint8_t  brightness;
    uint16_t bg_scroll[8];
    uint8_t  bgofs_ppu1_latch;
    uint8_t  bgofs_ppu2_latch;
    uint8_t  mode7_latch;
    uint16_t mode7_hofs;
    uint16_t mode7_vofs;
    uint16_t mode7_a;
    uint16_t mode7_b;
    uint16_t mode7_c;
    uint16_t mode7_d;
    uint16_t mode7_x;
    uint16_t mode7_y;
    uint16_t ppu_fixed_color;
    uint8_t  oam_priority_rotation;
    uint16_t ophct_latch;
    uint16_t opvct_latch;
    uint8_t  ophct_high;
    uint8_t  opvct_high;
    uint8_t  counters_latched;

    /* DMA / HDMA */
    uint8_t  dma_enable_mask;
    uint8_t  hdma_enable_mask;
    uint8_t  hdma_active_mask;
    uint8_t  hdma_completed_mask;
    uint8_t  hdma_do_transfer[8];
    uint8_t  hdma_line_counter[8];
    uint16_t hdma_table_address[8];
    uint16_t hdma_indirect_address[8];

    /* CPU I/O, Timers, and DMA registers ($4200-$43FF) */
    uint8_t  cpu_io_registers[0x200];
    uint8_t  nmitimen;
    uint8_t  nmi_flag;
    uint8_t  irq_flag;
    uint16_t htime_target;
    uint16_t vtime_target;
    uint16_t autojoy_result[4];
    uint8_t  autojoy_busy;
    uint32_t autojoy_remaining_clocks;

    /* Controllers */
    uint16_t controller_state[2];
    uint16_t controller_shift[2];
    uint8_t  controller_strobe;
    uint8_t  cpu_mdr;

    /* NTSC Scheduler & Beam Timing */
    uint16_t hcounter_master;
    uint16_t vcounter_line;
    uint8_t  field;
    uint16_t line_period;
    uint16_t refresh_position;
    uint8_t  refresh_done;
    uint8_t  scanline_render_done;
    uint8_t  nmi_pending;
    uint8_t  irq_pending;
    uint8_t  irq_line;
    uint8_t  in_interrupt;
    uint8_t  cpu_waiting;
    uint8_t  nmi_issued_this_vblank;
    uint64_t master_clock;
    uint64_t instruction_count;
    uint64_t frame_count;
    uint8_t  frame_ready;

    /* Framebuffer output (256x224 BGR555) */
    uint16_t framebuffer[TOPGEAR2_FRAME_WIDTH * TOPGEAR2_FRAME_HEIGHT];

    /* Definitive Edition enhancements */
    Tg2AspectMode aspect_mode;
    Tg2VisualMode visual_mode;
    int           car_paint_p1;
    int           car_paint_p2;
    uint8_t       car_paint_rgb_p1[3];
    uint8_t       car_paint_rgb_p2[3];
    uint16_t      car_shades_p1_bg1[5];
    uint16_t      car_shades_p1_oam[4];
    uint16_t      car_shades_p2_oam[4];
    Tg2ScanlineState scanline_state[TOPGEAR2_FRAME_HEIGHT];
    uint8_t       emissive_mask[TOPGEAR2_FRAME_WIDTH_16_9 * TOPGEAR2_FRAME_HEIGHT];
    uint16_t      framebuffer_wide[TOPGEAR2_FRAME_WIDTH_16_9 * TOPGEAR2_FRAME_HEIGHT];

    /* APU & Audio Subsystem */
    struct Apu *apu;
    uint64_t   last_apu_master_clock;
    double     apu_catchup_cycles;

    /* Dynamic Fallback Tier (interp816) */
    struct Interp816 *interp;
    bool interp_initialized;

    /* Widescreen off-edge sprite capture (extends past SNES [0..255]) */
    Tg2WsSprite   ws_staging[256];
    uint16_t      ws_staging_count;
    Tg2WsSprite   ws_sprites[256];
    uint16_t      ws_sprite_count;
    uint8_t       ws_staging_dirty;
};

/* Core register & flag helpers */
void tg2_set_flag(TopGear2Recomp *instance, uint8_t flag, int enabled);
int  tg2_flag(const TopGear2Recomp *instance, uint8_t flag);
void tg2_set_nz8(TopGear2Recomp *instance, uint8_t value);
void tg2_set_nz16(TopGear2Recomp *instance, uint16_t value);
uint8_t tg2_acc8(const TopGear2Recomp *instance);
uint16_t tg2_acc16(const TopGear2Recomp *instance);
void tg2_set_acc8(TopGear2Recomp *instance, uint8_t value);
void tg2_set_acc16(TopGear2Recomp *instance, uint16_t value);
void tg2_set_index8(TopGear2Recomp *instance, uint16_t *reg, uint8_t value);
void tg2_set_index16(TopGear2Recomp *instance, uint16_t *reg, uint16_t value);
void tg2_compare8(TopGear2Recomp *instance, uint8_t left, uint8_t right);
void tg2_compare16(TopGear2Recomp *instance, uint16_t left, uint16_t right);
void tg2_adc8(TopGear2Recomp *instance, uint8_t value);
void tg2_adc16(TopGear2Recomp *instance, uint16_t value);
void tg2_sbc8(TopGear2Recomp *instance, uint8_t value);
void tg2_sbc16(TopGear2Recomp *instance, uint16_t value);

/* Memory Bus */
int tg2_bus_read8(TopGear2Recomp *instance, uint32_t address, uint8_t *value);
int tg2_bus_read16(TopGear2Recomp *instance, uint32_t address, uint16_t *value);
int tg2_bus_read24(TopGear2Recomp *instance, uint32_t address, uint32_t *value);
int tg2_bus_write8(TopGear2Recomp *instance, uint32_t address, uint8_t value);
int tg2_bus_write16(TopGear2Recomp *instance, uint32_t address, uint16_t value);
int tg2_bus_store16(TopGear2Recomp *instance, uint32_t address, uint16_t value);
int tg2_push8(TopGear2Recomp *instance, uint8_t value);
int tg2_push16(TopGear2Recomp *instance, uint16_t value);
int tg2_pull8(TopGear2Recomp *instance, uint8_t *value);
int tg2_pull16(TopGear2Recomp *instance, uint16_t *value);

/* Subsystem I/O */
int tg2_ppu_read(TopGear2Recomp *instance, uint16_t address, uint8_t *value);
int tg2_ppu_write(TopGear2Recomp *instance, uint16_t address, uint8_t value);
int tg2_cpu_io_read(TopGear2Recomp *instance, uint16_t address, uint8_t *value);
int tg2_dma_io_write(TopGear2Recomp *instance, uint16_t address, uint8_t value);

/* Context Key */
static inline uint32_t tg2_generated_context_key(const TopGear2Recomp *instance) {
    unsigned m = instance->cpu.e ? 1u : (unsigned)((instance->cpu.p & TG2_P_M) != 0u);
    unsigned x = instance->cpu.e ? 1u : (unsigned)((instance->cpu.p & TG2_P_X) != 0u);
    return ((uint32_t)instance->cpu.e << 27) | ((uint32_t)m << 26) | ((uint32_t)x << 25) |
           ((uint32_t)instance->cpu.pbr << 16) | instance->cpu.pc;
}

/* Dispatch and Execution Steps */
int tg2_generated_static_step(TopGear2Recomp *instance);
int tg2_fallback_step(TopGear2Recomp *instance);
int tg2_interp_step_with_hooks(TopGear2Recomp *instance);
int tg2_semantic_step(TopGear2Recomp *instance);

/* Scheduler & Video */
static inline int tg2_is_active_race(const TopGear2Recomp *i) {
    if (!i) return 0;
    if (i->cpu.pbr == 0x81u) return 1;
    return (i->ppu_registers[0x07u] == 0x78u && i->ppu_registers[0x08u] == 0x71u);
}
int tg2_advance_master_clocks(TopGear2Recomp *instance, uint32_t clocks);
int tg2_advance_current_instruction(TopGear2Recomp *instance, unsigned cycles);
int tg2_render_scanline(TopGear2Recomp *instance, uint16_t line);
int tg2_service_interrupts(TopGear2Recomp *instance);
int tg2_ws_try_capture_tile(TopGear2Recomp *instance, uint8_t size);
void tg2_update_car_paint_cache(TopGear2Recomp *instance);
int tg2_is_two_player_race(const TopGear2Recomp *instance);

/* Audio Subsystem */
void tg2_audio_init(TopGear2Recomp *instance);
void tg2_audio_reset(TopGear2Recomp *instance);
void tg2_audio_free(TopGear2Recomp *instance);
void tg2_audio_sync(TopGear2Recomp *instance, uint64_t target_clock);
uint8_t tg2_audio_cpu_read(TopGear2Recomp *instance, uint8_t port);
void tg2_audio_cpu_write(TopGear2Recomp *instance, uint8_t port, uint8_t value);
size_t tg2_audio_extract_samples(TopGear2Recomp *instance, int16_t *buffer, size_t max_samples);

#endif /* TOPGEAR2_INTERNAL_H */
