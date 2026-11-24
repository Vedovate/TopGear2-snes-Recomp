#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "topgear2.h"
#include "topgear2_internal.h"
#include "topgear2_menu.h"
#include "topgear2_sha256.h"

static void test_sha256_nist_abc(void) {
    uint8_t digest[32];
    char hex[65];
    tg2_sha256_compute((const uint8_t *)"abc", 3u, digest);
    tg2_sha256_hex(digest, hex);
    assert(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    printf("[PASS] SHA-256 NIST 'abc' vector verified.\n");
}

static void test_rom_verify_rejections(void) {
    char hex[65];
    assert(tg2_verify_rom_buffer(NULL, 0u, hex) == TG2_ROM_ERR_IO);
    assert(tg2_verify_rom_buffer((const uint8_t *)"1234", 4u, hex) == TG2_ROM_ERR_SIZE);

    uint8_t *zeros = (uint8_t *)calloc(1u, TOPGEAR2_ROM_SIZE);
    assert(zeros != NULL);
    assert(tg2_verify_rom_buffer(zeros, TOPGEAR2_ROM_SIZE, hex) == TG2_ROM_ERR_HASH);
    assert(strlen(hex) == 64u);
    memcpy(zeros + 0x7FC0u, "TOP GEAR 2", 10u);
    assert(tg2_verify_rom_buffer(zeros, TOPGEAR2_ROM_SIZE, hex) == TG2_ROM_ERR_HASH);
    free(zeros);

    uint8_t *oversized = (uint8_t *)calloc(1u, TOPGEAR2_ROM_SIZE + 16u);
    assert(oversized != NULL);
    assert(tg2_verify_rom_buffer(oversized, TOPGEAR2_ROM_SIZE + 16u, NULL) == TG2_ROM_ERR_SIZE);
    free(oversized);

    printf("[PASS] ROM buffer verification rejects invalid size and mismatched hash.\n");
}

static void test_load_rom_preserves_old_buffer_on_failure(void) {
    TopGear2Recomp *tg2 = tg2_create();
    assert(tg2 != NULL);

    uint8_t *valid_sized = (uint8_t *)malloc(TOPGEAR2_ROM_SIZE);
    assert(valid_sized != NULL);
    memset(valid_sized, 0xEA, TOPGEAR2_ROM_SIZE);

    assert(tg2_load_rom(tg2, valid_sized, TOPGEAR2_ROM_SIZE) == true);
    assert(tg2->rom != NULL);
    assert(tg2->rom_size == TOPGEAR2_ROM_SIZE);
    assert(tg2->rom[0] == 0xEAu);
    free(valid_sized);

    const char *bad_path = "tests/test_bad_rom.sfc";
    FILE *fb = fopen(bad_path, "wb");
    assert(fb != NULL);
    fwrite("BADROM", 1u, 6u, fb);
    fclose(fb);

    assert(tg2_load_rom(tg2, NULL, 16u) == false);
    assert(tg2_load_rom(tg2, (const uint8_t *)"x", 0u) == false);
    assert(tg2_load_rom_file_checked(tg2, bad_path, NULL) == TG2_ROM_ERR_SIZE);
    remove(bad_path);
    assert(tg2->rom != NULL);
    assert(tg2->rom_size == TOPGEAR2_ROM_SIZE);
    assert(tg2->rom[0] == 0xEAu);

    tg2_destroy(tg2);
    printf("[PASS] tg2_load_rom preserves existing ROM buffer on invalid load.\n");
}

static void test_menu_json_escape_and_scancodes(void) {
    const char *cfg_path = "tests/test_regressions_cfg.json";
    Tg2MenuState ms;
    tg2_menu_init(&ms);

    const char *complex_path = "C:\\Games\\Top \"Gear\" 2\\Top Gear 2 (USA).sfc";
    snprintf(ms.rom_path, sizeof(ms.rom_path), "%s", complex_path);
    ms.aspect_mode = TG2_ASPECT_4_3;
    ms.master_volume = 73;

    tg2_menu_save_config(&ms, cfg_path);

    Tg2MenuState loaded;
    tg2_menu_init(&loaded);
    assert(tg2_menu_load_config(&loaded, cfg_path) == true);
    assert(strcmp(loaded.rom_path, complex_path) == 0);
    assert(loaded.aspect_mode == TG2_ASPECT_4_3);
    assert(loaded.master_volume == 73);
    remove(cfg_path);

    /* Ensure two unknown scancodes render cleanly without static buffer aliasing */
    loaded.active = true;
    loaded.tab = TG2_MENU_TAB_CONTROLS;
    loaded.keys[0].scancode = 999;
    loaded.keys[0].alt_scancode = 998;
    uint32_t *argb = (uint32_t *)calloc(398u * 224u, sizeof(uint32_t));
    assert(argb != NULL);
    tg2_menu_render(&loaded, argb, 398, 224);

    int nonzero = 0;
    for (size_t i = 0; i < 398u * 224u; i++) {
        if ((argb[i] & 0x00FFFFFFu) != 0u) nonzero++;
    }
    assert(nonzero > 1000);
    free(argb);

    printf("[PASS] Menu JSON backslash/quote escaping and dual unknown scancode render verified.\n");
}

static void test_ppu_latches_and_counters(void) {
    TopGear2Recomp *tg2 = tg2_create();
    assert(tg2 != NULL);

    /* Test OAM word latch reset ($2102/$2103) and bit 7 priority rotation */
    tg2_ppu_write(tg2, 0x2104u, 0x12u); /* low byte -> sets oam_latch_pending */
    assert(tg2->oam_latch_pending == 1u);
    tg2_ppu_write(tg2, 0x2102u, 0x04u);
    assert(tg2->oam_latch_pending == 0u);
    tg2_ppu_write(tg2, 0x2103u, 0x81u); /* bit 7 priority rotation + bit 0 high word addr */
    assert(tg2->oam_priority_rotation == 1u);
    assert(tg2->oam_byte_address == ((1u << 9) | (0x04u << 1)));

    /* Test CGRAM latch reset ($2121) */
    tg2_ppu_write(tg2, 0x2122u, 0x34u);
    assert(tg2->cgram_latch_pending == 1u);
    tg2_ppu_write(tg2, 0x2121u, 0x08u);
    assert(tg2->cgram_latch_pending == 0u);
    assert(tg2->cgram_byte_address == 0x10u);

    /* Test H/V counter latching ($2137) and $213C/$213D/$213F reads */
    tg2->hcounter_master = 0x123u * 4u; /* dot 0x123 */
    tg2->vcounter_line = 0x0F5u;
    tg2->cpu_mdr = 0x5Au;
    tg2->field = 1u;

    uint8_t v = 0;
    tg2_ppu_read(tg2, 0x2137u, &v);
    assert(tg2->counters_latched == 1u);
    assert(v == 0x5Au);

    /* OPHCT low then high */
    tg2_ppu_read(tg2, 0x213Cu, &v);
    assert(v == 0x23u);
    tg2_ppu_read(tg2, 0x213Cu, &v);
    assert(v == ((0x5Au & 0xFEu) | 1u));

    /* OPVCT low then high */
    tg2_ppu_read(tg2, 0x213Du, &v);
    assert(v == 0xF5u);
    tg2_ppu_read(tg2, 0x213Du, &v);
    assert(v == (0x5Au & 0xFEu));

    /* STAT78 reports latched + interlace field then clears latch */
    tg2_ppu_read(tg2, 0x213Fu, &v);
    assert((v & 0xC0u) == 0xC0u);
    assert(tg2->counters_latched == 0u);
    assert(tg2->ophct_high == 0u);
    assert(tg2->opvct_high == 0u);

    tg2_destroy(tg2);
    printf("[PASS] PPU $2102/$2103/$2121 latches and $2137/$213C/$213D/$213F counters verified.\n");
}

static void test_widescreen_interp_hooks(void) {
    TopGear2Recomp *tg2 = tg2_create();
    assert(tg2 != NULL);

    uint8_t *synth_rom = (uint8_t *)malloc(TOPGEAR2_ROM_SIZE);
    assert(synth_rom != NULL);
    memset(synth_rom, 0xEA, TOPGEAR2_ROM_SIZE); /* Fill with NOP ($EA) */
    assert(tg2_load_rom(tg2, synth_rom, TOPGEAR2_ROM_SIZE) == true);
    free(synth_rom);

    tg2_reset(tg2);
    tg2->cpu.e = 0u;
    tg2->cpu.p = 0u;
    tg2->cpu.pbr = 0x8Bu;
    tg2->ppu_registers[0x07u] = 0x78u;
    tg2->ppu_registers[0x08u] = 0x71u;

    /* 1. 8B:A9C9 (CMP right clip bound: 0x0147 in 16:9 vs 0x0100 in 4:3) */
    tg2->aspect_mode = TG2_ASPECT_16_9;
    tg2->cpu.pc = 0xA9C9u;
    tg2->cpu.a = 0x0120u;
    tg2->cpu.p = 0u;
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.pc == 0xA9CCu);
    assert((tg2->cpu.p & TG2_P_C) == 0u);

    tg2->aspect_mode = TG2_ASPECT_4_3;
    tg2->cpu.pc = 0xA9C9u;
    tg2->cpu.a = 0x0120u;
    tg2->cpu.p = 0u;
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.pc == 0xA9CCu);
    assert((tg2->cpu.p & TG2_P_C) != 0u);

    /* 2. 8B:A9D4 (BMI widescreen left bound: -71 threshold) */
    tg2->aspect_mode = TG2_ASPECT_16_9;
    tg2->cpu.pc = 0xA9D4u;
    tg2->cpu.a = (uint16_t)(-50);
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.pc == 0xA9D6u);

    tg2->cpu.pc = 0xA9D4u;
    tg2->cpu.a = (uint16_t)(-80);
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.pc == 0xAA30u);

    /* 3. 8B:AA0E (LDA right clip clamp: 0x0147 in 16:9 vs 0x0100 in 4:3) */
    tg2->aspect_mode = TG2_ASPECT_16_9;
    tg2->cpu.pc = 0xAA0Eu;
    tg2->cpu.a = 0u;
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.a == 0x0147u);
    assert(tg2->cpu.pc == 0xAA11u);

    tg2->aspect_mode = TG2_ASPECT_4_3;
    tg2->cpu.pc = 0xAA0Eu;
    tg2->cpu.a = 0u;
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.a == 0x0100u);
    assert(tg2->cpu.pc == 0xAA11u);

    /* 4. 8B:A7C1 (PREPEND early branch when dp+$34 == 0) */
    tg2->aspect_mode = TG2_ASPECT_16_9;
    tg2->cpu.d = 0x0100u;
    tg2->wram[0x0134u] = 0u;
    tg2->wram[0x0135u] = 0u;
    tg2->cpu.pc = 0xA7C1u;
    tg2->cpu.a = (uint16_t)(-72);
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.pc == 0xA7D8u);

    tg2->cpu.pc = 0xA7C1u;
    tg2->cpu.a = (uint16_t)(-70);
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->cpu.pc == 0xA7C3u);

    /* 5. 8B:858F and 8B:D7E7 (PREPEND staging hooks + fallback to NOP at LoROM offset) */
    tg2->ws_staging_count = 12u;
    tg2->ws_staging_dirty = 1u;
    tg2->cpu.pc = 0x858Fu;
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->ws_staging_count == 0u);
    assert(tg2->ws_staging_dirty == 0u);
    assert(tg2->cpu.pc == 0x8590u);

    tg2->cpu.pc = 0xD7E7u;
    assert(tg2_interp_step_with_hooks(tg2) == 1);
    assert(tg2->ws_staging_dirty == 1u);
    assert(tg2->cpu.pc == 0xD7E8u);

    tg2_destroy(tg2);
    printf("[PASS] ROM-less 16:9 widescreen interpreter hooks verified.\n");
}

static void run_rom_integration_tests(const char *rom_path) {
    /* Verify direct load with SHA-256 */
    TopGear2Recomp *tg2 = tg2_create();
    assert(tg2 != NULL);

    char sha_hex[65];
    Tg2RomStatus st = tg2_load_rom_file_checked(tg2, rom_path, sha_hex);
    if (st != TG2_ROM_OK) {
        fprintf(stderr, "FAIL: Could not load verified ROM '%s' (status=%d)\n", rom_path, (int)st);
        tg2_destroy(tg2);
        exit(1);
    }
    assert(strcmp(sha_hex, TOPGEAR2_USA_ROM_SHA256) == 0);

    /* Test 512-byte SMC header stripping */
    const char *smc_path = "tests/test_regressions_smc.sfc";
    uint8_t *smc_buf = (uint8_t *)calloc(1u, TOPGEAR2_ROM_SIZE + 512u);
    assert(smc_buf != NULL);
    memcpy(smc_buf + 512u, tg2->rom, TOPGEAR2_ROM_SIZE);
    FILE *fsmc = fopen(smc_path, "wb");
    assert(fsmc != NULL);
    assert(fwrite(smc_buf, 1u, TOPGEAR2_ROM_SIZE + 512u, fsmc) == TOPGEAR2_ROM_SIZE + 512u);
    fclose(fsmc);
    free(smc_buf);

    TopGear2Recomp *tg2_smc = tg2_create();
    assert(tg2_smc != NULL);
    char smc_hex[65];
    assert(tg2_load_rom_file_checked(tg2_smc, smc_path, smc_hex) == TG2_ROM_OK);
    assert(strcmp(smc_hex, TOPGEAR2_USA_ROM_SHA256) == 0);
    tg2_destroy(tg2_smc);
    remove(smc_path);
    printf("[PASS] 512-byte SMC header strip + SHA-256 verification passed.\n");

    /* Run 60 frames, dirty state, test full tg2_reset() */
    tg2_reset(tg2);
    for (int f = 0; f < 60; f++) {
        assert(tg2_step_frame(tg2, 0, 0) != 0);
    }
    tg2->wram[0x1234] = 0xAAu;
    tg2->vram[0x0200] = 0x55u;
    tg2->vram_increment_words = 128u;
    tg2->ws_staging_count = 7u;
    tg2->ws_sprite_count = 4u;
    tg2->ws_staging_dirty = 1u;

    tg2_reset(tg2);
    assert(tg2->frame_count == 0u);
    assert(tg2->wram[0x1234] == 0u);
    assert(tg2->vram[0x0200] == 0u);
    assert(tg2->vram_increment_words == 1u);
    assert(tg2->ws_staging_count == 0u);
    assert(tg2->ws_sprite_count == 0u);
    assert(tg2->ws_staging_dirty == 0u);

    /* Ensure clean boot after reset and test instant aspect re-render */
    for (int f = 0; f < 60; f++) {
        assert(tg2_step_frame(tg2, 0, 0) != 0);
    }

    tg2_set_aspect_mode(tg2, TG2_ASPECT_4_3);
    int w43 = 0, h43 = 0;
    tg2_get_active_frame_size(tg2, &w43, &h43);
    assert(w43 == 256 && h43 == 224);
    const uint16_t *fb43 = tg2_get_framebuffer_bgr555(tg2);
    int nz43 = 0;
    for (int i = 0; i < w43 * h43; i++) if (fb43[i] != 0u) nz43++;
    assert(nz43 > 1000);

    tg2_set_aspect_mode(tg2, TG2_ASPECT_16_9);
    int w169 = 0, h169 = 0;
    tg2_get_active_frame_size(tg2, &w169, &h169);
    assert(w169 == 398 && h169 == 224);
    const uint16_t *fb169 = tg2_get_framebuffer_bgr555(tg2);
    int nz169 = 0;
    for (int i = 0; i < w169 * h169; i++) if (fb169[i] != 0u) nz169++;
    assert(nz169 > 1000);
    printf("[PASS] Full tg2_reset() and instant aspect re-render verified.\n");

    /* Save State v2 truncated file atomic rollback test */
    const char *good_state = "tests/test_regressions_good.bin";
    const char *bad_state = "tests/test_regressions_bad.bin";
    uint64_t saved_fc = tg2->frame_count;
    assert(tg2_save_state_file(tg2, good_state) == true);

    FILE *fg = fopen(good_state, "rb");
    assert(fg != NULL);
    fseek(fg, 0, SEEK_END);
    long full_sz = ftell(fg);
    assert(full_sz > 1024);
    fseek(fg, 0, SEEK_SET);
    size_t half_sz = (size_t)(full_sz / 2);
    uint8_t *half_buf = (uint8_t *)malloc(half_sz);
    assert(half_buf != NULL);
    assert(fread(half_buf, 1u, half_sz, fg) == half_sz);
    fclose(fg);

    FILE *fb_out = fopen(bad_state, "wb");
    assert(fb_out != NULL);
    assert(fwrite(half_buf, 1u, half_sz, fb_out) == half_sz);
    fclose(fb_out);
    free(half_buf);

    for (int f = 0; f < 5; f++) {
        assert(tg2_step_frame(tg2, 0, 0) != 0);
    }
    uint64_t pre_bad_fc = tg2->frame_count;
    uint16_t pre_bad_pc = tg2->cpu.pc;
    assert(pre_bad_fc == saved_fc + 5u);

    /* Loading truncated save must fail AND preserve live state */
    assert(tg2_load_state_file(tg2, bad_state) == false);
    assert(tg2->frame_count == pre_bad_fc);
    assert(tg2->cpu.pc == pre_bad_pc);

    /* Valid save restores cleanly */
    assert(tg2_load_state_file(tg2, good_state) == true);
    assert(tg2->frame_count == saved_fc);

    remove(good_state);
    remove(bad_state);
    printf("[PASS] Save State v2 rollback on corrupted file verified.\n");

    tg2_destroy(tg2);
}

int main(int argc, char **argv) {
    printf("========================================================\n");
    printf("   Top Gear 2 Recomp - Regression & Core Verification   \n");
    printf("========================================================\n");

    test_sha256_nist_abc();
    test_rom_verify_rejections();
    test_load_rom_preserves_old_buffer_on_failure();
    test_menu_json_escape_and_scancodes();
    test_ppu_latches_and_counters();
    test_widescreen_interp_hooks();

    const char *rom_path = NULL;
    if (argc > 1 && argv[1] && argv[1][0]) {
        rom_path = argv[1];
    } else {
        const char *env_rom = getenv("TG2_ROM_PATH");
        if (env_rom && env_rom[0]) rom_path = env_rom;
    }

    if (rom_path) {
        printf("Running ROM integration checks using '%s'...\n", rom_path);
        run_rom_integration_tests(rom_path);
    } else {
        printf("[SKIP] ROM integration tests skipped (no ROM path argument or TG2_ROM_PATH).\n");
    }

    printf("\nAll regression checks PASSED!\n");
    return 0;
}
