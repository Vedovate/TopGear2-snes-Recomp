#include "topgear2.h"
#include "topgear2_internal.h"
#include "topgear2_platform.h"
#include "topgear2_sha256.h"
#include "snesrecomp_repo/runner/src/snes/interp816.h"
#include "snesrecomp_repo/runner/src/snes/apu.h"
#include "snesrecomp_repo/runner/src/snes/dsp.h"
#include "snesrecomp_repo/runner/src/snes/saveload.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

TopGear2Recomp* tg2_create(void) {
    TopGear2Recomp *instance = (TopGear2Recomp*)calloc(1, sizeof(TopGear2Recomp));
    if (!instance) return NULL;

    instance->line_period = 1364u;
    instance->refresh_position = 538u;
    instance->aspect_mode = TG2_ASPECT_16_9;
    instance->visual_mode = TG2_VISUAL_ENHANCED_SMOOTH;
    instance->car_paint_p1 = 0;
    instance->car_paint_p2 = 0;
    tg2_update_car_paint_cache(instance);

    tg2_audio_init(instance);
    return instance;
}

void tg2_destroy(TopGear2Recomp *instance) {
    if (!instance) return;
    if (instance->interp) {
        interp816_free(instance->interp);
        instance->interp = NULL;
    }
    tg2_audio_free(instance);
    if (instance->rom) {
        free(instance->rom);
        instance->rom = NULL;
    }
    free(instance);
}

bool tg2_load_rom(TopGear2Recomp *instance, const uint8_t *rom_data, size_t size) {
    if (!instance || !rom_data || size == 0u) return false;
    uint8_t *next = (uint8_t*)malloc(size);
    if (!next) return false;

    memcpy(next, rom_data, size);
    if (instance->rom) free(instance->rom);
    instance->rom = next;
    instance->rom_size = size;
    return true;
}

Tg2RomStatus tg2_load_rom_file_checked(TopGear2Recomp *instance, const char *path, char *out_sha256_hex) {
    if (out_sha256_hex) out_sha256_hex[0] = '\0';
    if (!instance || !path || !path[0]) return TG2_ROM_ERR_IO;

    FILE *f = tg2_fopen_utf8(path, "rb");
    if (!f) return TG2_ROM_ERR_IO;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return TG2_ROM_ERR_IO; }
    long size = ftell(f);
    if (size <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return TG2_ROM_ERR_SIZE; }

    uint8_t *buf = (uint8_t*)malloc((size_t)size);
    if (!buf) { fclose(f); return TG2_ROM_ERR_ALLOC; }

    if (fread(buf, 1u, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return TG2_ROM_ERR_IO;
    }
    fclose(f);

    /* Strip SMC 512-byte header if present */
    const uint8_t *rom_start = buf;
    size_t rom_len = (size_t)size;
    if (((size_t)size % 1024u) == 512u) {
        rom_start += 512u;
        rom_len -= 512u;
    }

    Tg2RomStatus st = tg2_verify_rom_buffer(rom_start, rom_len, out_sha256_hex);
    if (st != TG2_ROM_OK) {
        free(buf);
        return st;
    }

    bool ok = tg2_load_rom(instance, rom_start, rom_len);
    free(buf);
    return ok ? TG2_ROM_OK : TG2_ROM_ERR_ALLOC;
}

bool tg2_load_rom_file(TopGear2Recomp *instance, const char *path) {
    return tg2_load_rom_file_checked(instance, path, NULL) == TG2_ROM_OK;
}

void tg2_reset(TopGear2Recomp *instance) {
    if (!instance || !instance->rom) return;

    memset(&instance->cpu, 0, offsetof(TopGear2Recomp, framebuffer) - offsetof(TopGear2Recomp, cpu));
    memset(instance->framebuffer, 0, sizeof(instance->framebuffer));
    memset(instance->framebuffer_wide, 0, sizeof(instance->framebuffer_wide));
    memset(instance->emissive_mask, 0, sizeof(instance->emissive_mask));
    memset(instance->scanline_state, 0, sizeof(instance->scanline_state));
    memset(instance->ws_staging, 0, sizeof(instance->ws_staging));
    instance->ws_staging_count = 0u;
    memset(instance->ws_sprites, 0, sizeof(instance->ws_sprites));
    instance->ws_sprite_count = 0u;
    instance->ws_staging_dirty = 0u;
    instance->interp_initialized = false;
    instance->vram_increment_words = 1u;

    /* Reset vector at $00:FFFC */
    uint8_t lo = 0, hi = 0;
    tg2_bus_read8(instance, 0x00FFFCu, &lo);
    tg2_bus_read8(instance, 0x00FFFDu, &hi);
    uint16_t reset_pc = (uint16_t)(lo | ((uint16_t)hi << 8));
    if (reset_pc == 0u) reset_pc = 0x8A38u;

    instance->cpu.pc  = reset_pc;
    instance->cpu.pbr = 0x00u;
    instance->cpu.dbr = 0x00u;
    instance->cpu.d   = 0x0000u;
    instance->cpu.s   = 0x01FFu;
    instance->cpu.e   = 1u;
    instance->cpu.p   = TG2_P_M | TG2_P_X | TG2_P_I;
    instance->cpu.a   = 0u;
    instance->cpu.x   = 0u;
    instance->cpu.y   = 0u;

    instance->master_clock = 186u;
    instance->hcounter_master = 186u;
    instance->vcounter_line = 0u;
    instance->field = 0u;
    instance->line_period = 1364u;
    instance->refresh_position = 538u;
    instance->instruction_count = 0u;
    instance->frame_count = 0u;
    instance->frame_ready = 0u;
    instance->cpu_waiting = 0u;

    tg2_audio_reset(instance);
}

int tg2_step_instruction(TopGear2Recomp *instance) {
    return tg2_semantic_step(instance);
}

int tg2_step_frame(TopGear2Recomp *instance, uint16_t p1_input, uint16_t p2_input) {
    if (!instance) return 0;

    instance->controller_state[0] = p1_input;
    instance->controller_state[1] = p2_input;

    instance->frame_ready = 0u;
    uint64_t max_insns = 150000u;
    uint64_t count = 0;

    while (!instance->frame_ready && count < max_insns) {
        if (!tg2_semantic_step(instance)) return 0;
        count++;
    }
    return instance->frame_ready ? 1 : 0;
}

const uint16_t* tg2_get_framebuffer_bgr555(const TopGear2Recomp *instance) {
    if (!instance) return NULL;
    return (instance->aspect_mode == TG2_ASPECT_16_9) ? instance->framebuffer_wide : instance->framebuffer;
}

void tg2_get_framebuffer_rgba8888(const TopGear2Recomp *instance, uint32_t *out_rgba) {
    if (!instance || !out_rgba) return;
    int w = (instance->aspect_mode == TG2_ASPECT_16_9) ? TOPGEAR2_FRAME_WIDTH_16_9 : TOPGEAR2_FRAME_WIDTH_4_3;
    const uint16_t *src = (instance->aspect_mode == TG2_ASPECT_16_9) ? instance->framebuffer_wide : instance->framebuffer;
    size_t count = (size_t)w * TOPGEAR2_FRAME_HEIGHT;

    for (size_t i = 0; i < count; i++) {
        uint16_t c = src[i];
        uint32_t r = (uint32_t)((c & 0x001Fu) << 3) | ((c & 0x001Cu) >> 2);
        uint32_t g = (uint32_t)(((c >> 5) & 0x001Fu) << 3) | (((c >> 5) & 0x001Cu) >> 2);
        uint32_t b = (uint32_t)(((c >> 10) & 0x001Fu) << 3) | (((c >> 10) & 0x001Cu) >> 2);
        out_rgba[i] = (uint32_t)(0xFF000000u | (b << 16) | (g << 8) | r);
    }
}

void tg2_set_aspect_mode(TopGear2Recomp *instance, Tg2AspectMode mode) {
    if (!instance) return;
    if (instance->aspect_mode != mode) {
        instance->aspect_mode = mode;
        if (instance->frame_count > 0u) {
            tg2_rerender_current_frame(instance);
        }
    }
}

Tg2AspectMode tg2_get_aspect_mode(const TopGear2Recomp *instance) {
    return instance ? instance->aspect_mode : TG2_ASPECT_16_9;
}

void tg2_get_active_frame_size(const TopGear2Recomp *instance, int *out_w, int *out_h) {
    if (out_w) {
        *out_w = (instance && instance->aspect_mode == TG2_ASPECT_16_9) ? TOPGEAR2_FRAME_WIDTH_16_9 : TOPGEAR2_FRAME_WIDTH_4_3;
    }
    if (out_h) {
        *out_h = TOPGEAR2_FRAME_HEIGHT;
    }
}

void tg2_set_visual_mode(TopGear2Recomp *instance, Tg2VisualMode mode) {
    if (instance) instance->visual_mode = mode;
}

Tg2VisualMode tg2_get_visual_mode(const TopGear2Recomp *instance) {
    return instance ? instance->visual_mode : TG2_VISUAL_ENHANCED_SMOOTH;
}

void tg2_set_car_paint(TopGear2Recomp *instance, int player, int preset_id, uint8_t custom_r, uint8_t custom_g, uint8_t custom_b) {
    if (!instance) return;
    if (player == 0 || player == 1) {
        instance->car_paint_p1 = preset_id;
        instance->car_paint_rgb_p1[0] = custom_r;
        instance->car_paint_rgb_p1[1] = custom_g;
        instance->car_paint_rgb_p1[2] = custom_b;
    } else if (player == 2) {
        instance->car_paint_p2 = preset_id;
        instance->car_paint_rgb_p2[0] = custom_r;
        instance->car_paint_rgb_p2[1] = custom_g;
        instance->car_paint_rgb_p2[2] = custom_b;
    }
    tg2_update_car_paint_cache(instance);
}

void tg2_get_car_paint(const TopGear2Recomp *instance, int player, int *out_preset, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b) {
    if (!instance) return;
    if (player == 0 || player == 1) {
        if (out_preset) *out_preset = instance->car_paint_p1;
        if (out_r) *out_r = instance->car_paint_rgb_p1[0];
        if (out_g) *out_g = instance->car_paint_rgb_p1[1];
        if (out_b) *out_b = instance->car_paint_rgb_p1[2];
    } else if (player == 2) {
        if (out_preset) *out_preset = instance->car_paint_p2;
        if (out_r) *out_r = instance->car_paint_rgb_p2[0];
        if (out_g) *out_g = instance->car_paint_rgb_p2[1];
        if (out_b) *out_b = instance->car_paint_rgb_p2[2];
    }
}

#pragma pack(push, 1)
typedef struct Tg2SaveHeader {
    char magic[8];     /* "TG2SAV1\0" */
    uint32_t version;  /* 2 */
    uint64_t frame_count;
    uint64_t instruction_count;
    uint32_t wram_size;
    uint32_t vram_size;
    uint32_t oam_size;
    uint32_t cgram_size;
    uint32_t apu_ram_size;
    uint32_t core_size;
} Tg2SaveHeader;
#pragma pack(pop)

typedef struct Tg2FileSaveLoad {
    SaveLoadInfo info;
    FILE *f;
    bool saving;
    bool ok;
} Tg2FileSaveLoad;

static void tg2_saveload_cb(SaveLoadInfo *sli, void *data, size_t data_size) {
    Tg2FileSaveLoad *ctx = (Tg2FileSaveLoad *)sli;
    if (!ctx->ok || data_size == 0) return;
    if (ctx->saving) {
        if (fwrite(data, 1, data_size, ctx->f) != data_size) ctx->ok = false;
    } else {
        if (fread(data, 1, data_size, ctx->f) != data_size) ctx->ok = false;
    }
}

bool tg2_save_state_json_summary(const TopGear2Recomp *instance, const char *json_path) {
    if (!instance || !json_path || !json_path[0]) return false;
    FILE *fj = tg2_fopen_utf8(json_path, "w");
    if (!fj) return false;

    uint16_t circuit = (uint16_t)(instance->wram[0x0108] | ((uint16_t)instance->wram[0x0109] << 8));
    uint16_t speed = (uint16_t)(instance->wram[0x04F0] | ((uint16_t)instance->wram[0x04F1] << 8));
    uint16_t rpm = (uint16_t)(instance->wram[0x04F2] | ((uint16_t)instance->wram[0x04F3] << 8));
    uint32_t money = (uint32_t)(instance->wram[0x17A0] | ((uint32_t)instance->wram[0x17A1] << 8) | ((uint32_t)instance->wram[0x17A2] << 16));

    fprintf(fj, "{\n");
    fprintf(fj, "  \"version\": 2,\n");
    fprintf(fj, "  \"frame_count\": %llu,\n", (unsigned long long)instance->frame_count);
    fprintf(fj, "  \"circuit_id\": %u,\n", (unsigned)circuit);
    fprintf(fj, "  \"country_index\": %u,\n", (unsigned)(circuit / 4u));
    fprintf(fj, "  \"race_index\": %u,\n", (unsigned)(circuit % 4u));
    fprintf(fj, "  \"speed_mph\": %u,\n", (unsigned)speed);
    fprintf(fj, "  \"rpm\": %u,\n", (unsigned)rpm);
    fprintf(fj, "  \"money\": %u\n", (unsigned)money);
    fprintf(fj, "}\n");
    fclose(fj);
    return true;
}

bool tg2_save_state_file(const TopGear2Recomp *instance, const char *path) {
    if (!instance || !path) return false;
    FILE *f = tg2_fopen_utf8(path, "wb");
    if (!f) return false;

    const size_t core_size = offsetof(TopGear2Recomp, framebuffer) - offsetof(TopGear2Recomp, cpu);

    Tg2SaveHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, "TG2SAV1\0", 8);
    hdr.version = 2u;
    hdr.frame_count = instance->frame_count;
    hdr.instruction_count = instance->instruction_count;
    hdr.wram_size = sizeof(instance->wram);
    hdr.vram_size = sizeof(instance->vram);
    hdr.oam_size = sizeof(instance->oam);
    hdr.cgram_size = sizeof(instance->cgram);
    hdr.apu_ram_size = (instance->apu) ? 65536u : 0u;
    hdr.core_size = (uint32_t)core_size;

    if (fwrite(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return false; }
    if (fwrite(&instance->cpu, core_size, 1, f) != 1) { fclose(f); return false; }
    if (fwrite(&instance->last_apu_master_clock, sizeof(instance->last_apu_master_clock), 1, f) != 1) { fclose(f); return false; }
    if (fwrite(&instance->apu_catchup_cycles, sizeof(instance->apu_catchup_cycles), 1, f) != 1) { fclose(f); return false; }

    /* Full APU state (RAM, ports, timers, DSP, SPC700 registers/PC) */
    if (hdr.apu_ram_size > 0 && instance->apu) {
        Tg2FileSaveLoad sli;
        sli.info.func = tg2_saveload_cb;
        sli.info.peek = NULL;
        sli.f = f;
        sli.saving = true;
        sli.ok = true;
        apu_saveload((Apu *)(uintptr_t)instance->apu, &sli.info);
        if (!sli.ok) { fclose(f); return false; }
    }

    fclose(f);
    return true;
}

bool tg2_load_state_file(TopGear2Recomp *instance, const char *path) {
    if (!instance || !path) return false;
    FILE *f = tg2_fopen_utf8(path, "rb");
    if (!f) return false;

    const size_t core_size = offsetof(TopGear2Recomp, framebuffer) - offsetof(TopGear2Recomp, cpu);

    Tg2SaveHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return false; }
    if (memcmp(hdr.magic, "TG2SAV1\0", 8) != 0 ||
        hdr.version != 2u ||
        hdr.core_size != (uint32_t)core_size ||
        hdr.wram_size != sizeof(instance->wram) ||
        hdr.vram_size != sizeof(instance->vram) ||
        hdr.oam_size != sizeof(instance->oam) ||
        hdr.cgram_size != sizeof(instance->cgram)) {
        fclose(f);
        return false;
    }

    uint8_t *new_core = (uint8_t*)malloc(core_size);
    uint8_t *backup_core = (uint8_t*)malloc(core_size);
    if (!new_core || !backup_core) {
        free(new_core);
        free(backup_core);
        fclose(f);
        return false;
    }

    uint64_t new_last_apu = 0u;
    double new_catchup = 0.0;
    if (fread(new_core, core_size, 1, f) != 1 ||
        fread(&new_last_apu, sizeof(new_last_apu), 1, f) != 1 ||
        fread(&new_catchup, sizeof(new_catchup), 1, f) != 1) {
        free(new_core);
        free(backup_core);
        fclose(f);
        return false;
    }

    memcpy(backup_core, &instance->cpu, core_size);
    uint64_t backup_last_apu = instance->last_apu_master_clock;
    double backup_catchup = instance->apu_catchup_cycles;

    memcpy(&instance->cpu, new_core, core_size);
    instance->last_apu_master_clock = new_last_apu;
    instance->apu_catchup_cycles = new_catchup;
    free(new_core);

    if (hdr.apu_ram_size > 0 && instance->apu) {
        Tg2FileSaveLoad sli;
        sli.info.func = tg2_saveload_cb;
        sli.info.peek = NULL;
        sli.f = f;
        sli.saving = false;
        sli.ok = true;
        apu_saveload(instance->apu, &sli.info);
        if (!sli.ok) {
            memcpy(&instance->cpu, backup_core, core_size);
            instance->last_apu_master_clock = backup_last_apu;
            instance->apu_catchup_cycles = backup_catchup;
            free(backup_core);
            fclose(f);
            return false;
        }
        apu_clearPortQueue(instance->apu);
    }

    free(backup_core);
    fclose(f);

    instance->frame_count = hdr.frame_count;
    instance->instruction_count = hdr.instruction_count;
    memset(instance->ws_staging, 0, sizeof(instance->ws_staging));
    instance->ws_staging_count = 0u;
    memset(instance->ws_sprites, 0, sizeof(instance->ws_sprites));
    instance->ws_sprite_count = 0u;
    instance->ws_staging_dirty = 0u;
    instance->interp_initialized = false;
    return true;
}

size_t tg2_get_audio_samples(TopGear2Recomp *instance, int16_t *out_pcm, size_t max_samples) {
    return tg2_audio_extract_samples(instance, out_pcm, max_samples);
}

void tg2_get_cpu_state(const TopGear2Recomp *instance, TopGear2CpuState *out_state) {
    if (!instance || !out_state) return;
    out_state->a   = instance->cpu.a;
    out_state->x   = instance->cpu.x;
    out_state->y   = instance->cpu.y;
    out_state->s   = instance->cpu.s;
    out_state->d   = instance->cpu.d;
    out_state->pc  = instance->cpu.pc;
    out_state->p   = instance->cpu.p;
    out_state->e   = instance->cpu.e;
    out_state->pbr = instance->cpu.pbr;
    out_state->dbr = instance->cpu.dbr;
}

void tg2_set_cpu_state(TopGear2Recomp *instance, const TopGear2CpuState *state) {
    if (!instance || !state) return;
    instance->cpu.a   = state->a;
    instance->cpu.x   = state->x;
    instance->cpu.y   = state->y;
    instance->cpu.s   = state->s;
    instance->cpu.d   = state->d;
    instance->cpu.pc  = state->pc;
    instance->cpu.p   = state->p;
    instance->cpu.e   = state->e;
    instance->cpu.pbr = state->pbr;
    instance->cpu.dbr = state->dbr;
}

bool tg2_jump_to_race_bank81(TopGear2Recomp *instance) {
    if (!instance) return false;
    instance->cpu.pbr = 0x9Fu;
    instance->cpu.pc  = 0x817Fu;
    instance->cpu.dbr = 0x9Fu;
    instance->cpu.d   = 0x0100u;
    instance->cpu.s   = 0x1FFFu;
    instance->cpu.e   = 0u;
    instance->cpu.p   = 0u;
    for (int step = 0; step < 5; step++) {
        if (!tg2_step_instruction(instance)) return false;
    }
    return (instance->cpu.pbr == 0x81 && instance->cpu.pc == 0x8045);
}

uint64_t tg2_get_frame_count(const TopGear2Recomp *instance) {
    return instance ? instance->frame_count : 0;
}

uint64_t tg2_get_instruction_count(const TopGear2Recomp *instance) {
    return instance ? instance->instruction_count : 0;
}

const uint8_t* tg2_get_wram(const TopGear2Recomp *instance) {
    return instance ? instance->wram : NULL;
}

uint8_t* tg2_get_wram_rw(TopGear2Recomp *instance) {
    return instance ? instance->wram : NULL;
}

const uint8_t* tg2_get_vram(const TopGear2Recomp *instance) {
    return instance ? instance->vram : NULL;
}

const uint8_t* tg2_get_cgram(const TopGear2Recomp *instance) {
    return instance ? instance->cgram : NULL;
}

const uint8_t* tg2_get_oam(const TopGear2Recomp *instance) {
    return instance ? instance->oam : NULL;
}

const uint8_t* tg2_get_ppu_registers(const TopGear2Recomp *instance) {
    return instance ? instance->ppu_registers : NULL;
}

const uint8_t* tg2_get_apu_ram(const TopGear2Recomp *instance) {
    if (!instance || !instance->apu) return NULL;
    return instance->apu->ram;
}

uint16_t tg2_get_dsp_dir_page(const TopGear2Recomp *instance) {
    if (!instance || !instance->apu || !instance->apu->dsp) return 0;
    return instance->apu->dsp->dirPage;
}
