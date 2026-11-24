#include "topgear2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void save_ppm(const char *filename, const uint32_t *argb, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; i++) {
        uint32_t c = argb[i];
        uint8_t rgb[3];
        rgb[0] = (uint8_t)((c >> 16) & 0xFF);
        rgb[1] = (uint8_t)((c >> 8) & 0xFF);
        rgb[2] = (uint8_t)(c & 0xFF);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    printf("========================================================\n");
    printf("  Testing Top Gear 2: Definitive Edition Subsystems     \n");
    printf("========================================================\n");

    TopGear2Recomp *tg2 = tg2_create();
    assert(tg2 != NULL);

    const char *rom_path = getenv("TG2_ROM_PATH");
    if (!rom_path || !rom_path[0]) rom_path = "Top Gear 2 (USA).sfc";
    if (argc > 1) rom_path = argv[1];

    bool loaded = tg2_load_rom_file(tg2, rom_path);
    assert(loaded);

    tg2_reset(tg2);
    printf("[1/5] ROM loaded and CPU reset OK.\n");

    /* Default settings check: 16:9 Widescreen & Enhanced HD */
    assert(tg2_get_aspect_mode(tg2) == TG2_ASPECT_16_9);
    assert(tg2_get_visual_mode(tg2) == TG2_VISUAL_ENHANCED_SMOOTH);
    int def_w = 0, def_h = 0;
    tg2_get_active_frame_size(tg2, &def_w, &def_h);
    assert(def_w == 398);
    assert(def_h == 224);
    printf("[2/5] Default Aspect 16:9 (398x224) and Enhanced Mode verified.\n");

    printf("Advancing through menus to Auckland active race...\n");
    bool screen_faded_in = false;
    for (int f = 1; f <= 1500; f++) {
        uint16_t pad = 0;
        if (f >= 200 && f <= 210) pad = TG2_KEY_START; // Title -> Main Menu
        if (f >= 390 && f <= 395) pad = TG2_KEY_DOWN;  // Down to START
        if (f >= 420 && f <= 425) pad = TG2_KEY_START; // Select START -> Country Select
        if (f >= 650 && f <= 655) pad = TG2_KEY_START; // Select RACE
        if (f >= 850 && f <= 860) pad = TG2_KEY_START; // Confirm Track
        if (f >= 950 && f <= 960) pad = TG2_KEY_START; // Confirm Car
        if (f >= 1050 && f <= 1060) pad = TG2_KEY_START; // Launch Race Grid

        int ok = tg2_step_frame(tg2, pad, 0);
        assert(ok);

        const uint8_t *ppu = tg2_get_ppu_registers(tg2);
        if (ppu && ppu[0x07] == 0x78 && ppu[0x08] == 0x71 && (ppu[0x00] & 0x80u) == 0 && (ppu[0x00] & 0x0Fu) >= 10) {
            printf("Active race fully visible at frame %d! BG1SC=%02X BG2SC=%02X INID=%02X\n",
                   f, ppu[0x07], ppu[0x08], ppu[0x00]);
            screen_faded_in = true;
            break;
        }

        if (f % 300 == 0) {
            printf("  Frame %4d completed...\n", f);
            fflush(stdout);
        }
    }
    assert(screen_faded_in);

    /* Advance 10 more frames in full race view */
    for (int f = 0; f < 10; f++) {
        tg2_step_frame(tg2, 0, 0);
    }

    TopGear2CpuState cpu;
    tg2_get_cpu_state(tg2, &cpu);
    const uint8_t *ppu = tg2_get_ppu_registers(tg2);
    printf("Active Race State: PBR=%02X, PC=%04X, BG1SC=%02X, BG2SC=%02X, BGMODE=%02X, INID=%02X\n",
           cpu.pbr, cpu.pc, ppu ? ppu[0x07] : 0, ppu ? ppu[0x08] : 0, ppu ? ppu[0x05] : 0, ppu ? ppu[0x00] : 0);
    fflush(stdout);

    /* Verify 16:9 Widescreen Framebuffer contents */
    const uint16_t *fb16_9 = tg2_get_framebuffer_bgr555(tg2);
    assert(fb16_9 != NULL);

    /* Check that left wing (x=10..60) and right wing (x=335..385) have active pixels (not black) */
    int active_left_wing = 0;
    int active_right_wing = 0;
    for (int y = 50; y < 200; y++) {
        for (int x = 10; x < 60; x++) {
            if (fb16_9[y * 398 + x] != 0) active_left_wing++;
        }
        for (int x = 335; x < 385; x++) {
            if (fb16_9[y * 398 + x] != 0) active_right_wing++;
        }
    }
    printf("      Left Widescreen Wing active pixels:  %d\n", active_left_wing);
    printf("      Right Widescreen Wing active pixels: %d\n", active_right_wing);
    fflush(stdout);
    assert(active_left_wing > 500);
    assert(active_right_wing > 500);
    printf("[3/5] Widescreen 16:9 field of view verified.\n");

    /* Verify HD Visual Modes and generate PPM dumps */
    uint32_t *hd_buf = (uint32_t*)malloc(TOPGEAR2_MAX_FB_WIDTH * TOPGEAR2_MAX_FB_HEIGHT * sizeof(uint32_t));
    assert(hd_buf != NULL);

    int w = 0, h = 0;

    /* Mode 1: Pixel Puro */
    tg2_set_visual_mode(tg2, TG2_VISUAL_PIXEL_PURO);
    tg2_render_hd_frame(tg2, hd_buf, &w, &h);
    assert(w == 398 && h == 224);
    save_ppm("tests/test_de_pixel.ppm", hd_buf, w, h);

    /* Mode 2: CRT Arcade */
    tg2_set_visual_mode(tg2, TG2_VISUAL_CRT_ARCADE);
    tg2_render_hd_frame(tg2, hd_buf, &w, &h);
    assert(w == 796 && h == 448);
    save_ppm("tests/test_de_crt.ppm", hd_buf, w, h);

    /* Mode 3: Enhanced HD with Sub-Scanline Curves & Bloom */
    tg2_set_visual_mode(tg2, TG2_VISUAL_ENHANCED_SMOOTH);
    tg2_render_hd_frame(tg2, hd_buf, &w, &h);
    assert(w == 796 && h == 448);
    save_ppm("tests/test_de_enhanced.ppm", hd_buf, w, h);

    /* Mode 4: 4:3 Aspect Toggle */
    tg2_set_aspect_mode(tg2, TG2_ASPECT_4_3);
    tg2_render_hd_frame(tg2, hd_buf, &w, &h);
    assert(w == 512 && h == 448);
    save_ppm("tests/test_de_4_3.ppm", hd_buf, w, h);

    /* Restore 16:9 */
    tg2_set_aspect_mode(tg2, TG2_ASPECT_16_9);
    printf("[4/5] All 3 Visual Modes (Pixel Puro, CRT, Enhanced HD) and 4:3 toggle verified.\n");

    /* Test Car Paint Customization */
    tg2_set_car_paint(tg2, 1, 3, 255, 128, 0); /* Laranja McLaren */
    int cur_preset = 0;
    uint8_t cr = 0, cg = 0, cb = 0;
    tg2_get_car_paint(tg2, 1, &cur_preset, &cr, &cg, &cb);
    assert(cur_preset == 3);

    /* Advance 1 frame to re-render with new car paint */
    tg2_step_frame(tg2, 0, 0);
    tg2_render_hd_frame(tg2, hd_buf, &w, &h);
    save_ppm("tests/test_de_mclaren.ppm", hd_buf, w, h);

    /* Test Save State and Auto-Load */
    uint64_t saved_frame = tg2_get_frame_count(tg2);
    bool saved = tg2_save_state_file(tg2, "tests/test_quicksave.bin");
    assert(saved);

    /* Explicitly export telemetry JSON summary */
    bool json_ok = tg2_save_state_json_summary(tg2, "tests/test_savegame.json");
    assert(json_ok);

    /* Verify JSON was written */
    FILE *fj = fopen("tests/test_savegame.json", "r");
    assert(fj != NULL);
    char json_buf[512];
    size_t rj = fread(json_buf, 1, sizeof(json_buf) - 1, fj);
    json_buf[rj] = '\0';
    fclose(fj);
    assert(strstr(json_buf, "circuit_id") != NULL);
    assert(strstr(json_buf, "speed_mph") != NULL);
    remove("tests/test_savegame.json");

    /* Advance 10 more frames */
    for (int f = 0; f < 10; f++) {
        tg2_step_frame(tg2, 0, 0);
    }
    assert(tg2_get_frame_count(tg2) == saved_frame + 10);

    /* Load state back */
    bool restored = tg2_load_state_file(tg2, "tests/test_quicksave.bin");
    assert(restored);
    assert(tg2_get_frame_count(tg2) == saved_frame);
    printf("[5/5] Car customization, Save States, and savegame.json telemetry verified.\n");

    free(hd_buf);
    tg2_destroy(tg2);

    printf("\nAll Definitive Edition Subsystems PASSED with 100%% Success!\n");
    return 0;
}
