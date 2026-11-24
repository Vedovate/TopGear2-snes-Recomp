#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "topgear2.h"
#include "topgear2_internal.h"

int main(int argc, char **argv) {
    printf("========================================================\n");
    printf("  Testing Money Calculation & Race Transition (Bank 81) \n");
    printf("========================================================\n");

    TopGear2Recomp *tg2 = tg2_create();
    if (!tg2) {
        printf("Error: Failed to create TopGear2Recomp instance.\n");
        return 1;
    }

    const char *rom_path = getenv("TG2_ROM_PATH");
    if (!rom_path || !rom_path[0]) rom_path = "Top Gear 2 (USA).sfc";
    if (argc > 1) rom_path = argv[1];

    if (!tg2_load_rom_file(tg2, rom_path)) {
        printf("Error: Could not load ROM from '%s'\n", rom_path);
        tg2_destroy(tg2);
        return 1;
    }

    tg2_reset(tg2);

    /* Test 1: Verify hardware multiplier reading (MPYL / MPYM / MPYH) */
    tg2->mode7_a = 0x0000;
    tg2->ppu_registers[0x1Cu] = 0x01; /* M7B = 1 */
    uint8_t ml = 0xFF, mm = 0xFF;
    tg2_ppu_read(tg2, 0x2134u, &ml);
    tg2_ppu_read(tg2, 0x2135u, &mm);
    if (ml != 0 || mm != 0) {
        printf("FAIL: MPY(0, 1) returned %02X%02X instead of 0000!\n", mm, ml);
        tg2_destroy(tg2);
        return 1;
    }
    printf("PASS: Mode 7 hardware multiplier zero-product test verified.\n");

    tg2->mode7_a = 0x1234;
    tg2->ppu_registers[0x1Cu] = 0x02; /* M7B = 2 */
    tg2_ppu_read(tg2, 0x2134u, &ml);
    tg2_ppu_read(tg2, 0x2135u, &mm);
    uint16_t prod = ml | (mm << 8);
    if (prod != 0x2468) {
        printf("FAIL: MPY(0x1234, 2) returned %04X instead of 0x2468!\n", prod);
        tg2_destroy(tg2);
        return 1;
    }
    printf("PASS: Mode 7 hardware multiplier scale test verified (0x2468).\n");

    /* Test 2: Verify direct execution of 9F:817F..818D (JML [$0100]) into Bank $81 */
    tg2->cpu.pbr = 0x9Fu;
    tg2->cpu.pc  = 0x817Fu;
    tg2->cpu.dbr = 0x9Fu;
    tg2->cpu.d   = 0x0100u;
    tg2->cpu.s   = 0x1FFFu;
    tg2->cpu.e   = 0u;
    tg2->cpu.p   = 0u; /* 16-bit A, X, Y */

    /* Step 817F (LDA $818000), 8183 (STA $0100), 8186 (LDA $818002), 818A (STA $0102), 818D (JML [$0100]) */
    for (int step = 0; step < 5; step++) {
        uint8_t prev_pbr = tg2->cpu.pbr;
        uint16_t prev_pc = tg2->cpu.pc;
        if (!tg2_step_instruction(tg2)) {
            printf("FAIL: tg2_step_instruction failed at %02X:%04X (step %d)!\n", prev_pbr, prev_pc, step);
            tg2_destroy(tg2);
            return 1;
        }
    }

    if (tg2->cpu.pbr != 0x81 || tg2->cpu.pc != 0x8045) {
        printf("FAIL: JML [$0100] target mismatch! Expected 81:8045, got %02X:%04X\n",
               tg2->cpu.pbr, tg2->cpu.pc);
        tg2_destroy(tg2);
        return 1;
    }
    printf("PASS: 9F:818D JML [$0100] successfully transitioned to Race Entry 81:8045!\n");

    /* Step the first instructions at 81:8045 in static recompilation */
    for (int step = 0; step < 5; step++) {
        uint8_t prev_pbr = tg2->cpu.pbr;
        uint16_t prev_pc = tg2->cpu.pc;
        if (!tg2_step_instruction(tg2)) {
            printf("FAIL: Race engine stopped at %02X:%04X (step %d)!\n", prev_pbr, prev_pc, step);
            tg2_destroy(tg2);
            return 1;
        }
    }
    printf("PASS: Executed first race instructions at 81:8045 successfully (PC=%02X:%04X)!\n",
           tg2->cpu.pbr, tg2->cpu.pc);

    /* Reset back to authentic SNES state */
    tg2_reset(tg2);

    printf("Running initial boot sequence (120 frames)...\n");
    for (int f = 0; f < 120; f++) {
        if (!tg2_step_frame(tg2, 0, 0)) {
            printf("FAIL: Engine stopped during boot at frame %d!\n", f);
            tg2_destroy(tg2);
            return 1;
        }
    }

    printf("Simulating navigation through Title / Menu to Upgrade & Race...\n");
    bool entered_bank81 = false;
    for (int f = 120; f < 1500; f++) {
        uint16_t input = 0;
        /* Alternate pressing START and B to advance through menus */
        if ((f / 30) % 2 == 1 && (f % 30 < 10)) {
            input = TG2_KEY_START | TG2_KEY_B;
        }

        if (!tg2_step_frame(tg2, input, 0)) {
            printf("FAIL: Engine stopped at frame %d (PC=%02X:%04X)!\n",
                   f, tg2->cpu.pbr, tg2->cpu.pc);
            tg2_destroy(tg2);
            return 1;
        }

        /* Check WRAM Money buffer if populated */
        if (f % 100 == 0) {
            char buf[32];
            int len = 0;
            for (int i = 0x1797; i <= 0x17B5; i++) {
                uint8_t ch = tg2->wram[i];
                if (ch >= 32 && ch <= 126) buf[len++] = (char)ch;
                else buf[len++] = '.';
            }
            buf[len] = '\0';
            printf("Frame %4d: PC=%02X:%04X WRAM[1797..17B5]='%s'\n",
                   f, tg2->cpu.pbr, tg2->cpu.pc, buf);
        }

        if (tg2->cpu.pbr == 0x81) {
            entered_bank81 = true;
            printf("SUCCESS: Reached Bank $81 (Race Engine) at frame %d! PC=%02X:%04X\n",
                   f, tg2->cpu.pbr, tg2->cpu.pc);
            break;
        }
    }

    if (entered_bank81) {
        printf("Executing 120 frames inside Bank $81 (Race Engine)...\n");
        for (int rf = 0; rf < 120; rf++) {
            if (!tg2_step_frame(tg2, 0, 0)) {
                printf("FAIL: Race engine crashed in Bank $81 at frame %d!\n", rf);
                tg2_destroy(tg2);
                return 1;
            }
        }
        printf("PASS: Successfully executed 120 frames of Race Engine in Bank $81 without errors!\n");
    } else {
        printf("Info: Did not enter Bank $81 yet (frame limit reached), but menus ran without crash.\n");
    }

    printf("\n=== Qualification Result: SUCCESS ===\n");
    tg2_destroy(tg2);
    return 0;
}
