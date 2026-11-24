#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "topgear2.h"
#include "topgear2_internal.h"

static void save_frame_ppm(const char *filename, const uint16_t *bgr555, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint16_t c = bgr555[y * width + x];
            uint8_t r = (uint8_t)(((c & 0x001F) * 255) / 31);
            uint8_t g = (uint8_t)((((c >> 5) & 0x001F) * 255) / 31);
            uint8_t b = (uint8_t)((((c >> 10) & 0x001F) * 255) / 31);
            fputc(r, f);
            fputc(g, f);
            fputc(b, f);
        }
    }
    fclose(f);
    printf("Saved frame to %s\n", filename);
}

int main(int argc, char **argv) {
    TopGear2Recomp *tg2 = tg2_create();
    if (!tg2) return 1;

    const char *rom_path = getenv("TG2_ROM_PATH");
    if (!rom_path || !rom_path[0]) rom_path = "Top Gear 2 (USA).sfc";
    if (argc > 1) rom_path = argv[1];
    if (!tg2_load_rom_file(tg2, rom_path)) {
        printf("Could not load ROM from '%s'\n", rom_path);
        tg2_destroy(tg2);
        return 1;
    }

    tg2_reset(tg2);

    int total_frames = 240;
    if (argc > 2) total_frames = atoi(argv[2]);

    printf("Executing initial instruction burst...\n");
    for (int i = 0; i < 50000; i++) {
        tg2_step_instruction(tg2);
    }

    printf("Running %d frames with diagnostic telemetry...\n", total_frames);

    for (int f = 1; f <= total_frames; f++) {
        if (!tg2_step_frame(tg2, 0, 0)) {
            printf("Frame %d failed!\n", f);
            break;
        }

        int fb_w = TOPGEAR2_FRAME_WIDTH;
        int fb_h = TOPGEAR2_FRAME_HEIGHT;
        tg2_get_active_frame_size(tg2, &fb_w, &fb_h);
        const uint16_t *fb = tg2_get_framebuffer_bgr555(tg2);
        int fb_nz = 0;
        for (int i = 0; i < fb_w * fb_h; i++) {
            if (fb[i]) fb_nz++;
        }

        if (f % 20 == 0 || f >= 90) {
            printf("Frame %3d: PC=%02X:%04X A=%04X DP=%04X WRAM[14]=%02X%02X nmitimen=%02X bright=%2d FB_nz=%5d\n",
                   f, tg2->cpu.pbr, tg2->cpu.pc, tg2->cpu.a, tg2->cpu.d,
                   tg2->wram[0x15], tg2->wram[0x14], tg2->nmitimen,
                   tg2->brightness, fb_nz);
        }

        if (f == 60 || f == 250 || f == 450) {
            char name[64];
            snprintf(name, sizeof(name), "tests/tg2_display_frame_%d.ppm", f);
            save_frame_ppm(name, fb, fb_w, fb_h);
        }
    }

    tg2_destroy(tg2);
    return 0;
}
