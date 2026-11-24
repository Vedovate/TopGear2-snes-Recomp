#include "topgear2_internal.h"
#include <string.h>

static uint16_t line_period(const TopGear2Recomp *i) {
    return (uint16_t)((i->field && i->vcounter_line == 240u) ? 1360u : 1364u);
}

static uint16_t refresh_position(const TopGear2Recomp *i) {
    return (uint16_t)(538u - ((uint8_t)i->master_clock & 7u));
}

static uint8_t hdma_mode_length(uint8_t mode) {
    static const uint8_t n[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    return n[mode & 7u];
}

static uint8_t hdma_bbus_offset(uint8_t mode, uint8_t index) {
    static const uint8_t p[8][4] = {
        {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
        {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1}
    };
    return p[mode & 7u][index & 3u];
}

static void complete_autojoy(TopGear2Recomp *i) {
    unsigned n;
    i->autojoy_result[0] = i->controller_state[0];
    i->autojoy_result[1] = i->controller_state[1];
    i->autojoy_result[2] = 0u;
    i->autojoy_result[3] = 0u;
    for (n = 0; n < 4; n++) {
        i->cpu_io_registers[0x18u + n * 2u] = (uint8_t)i->autojoy_result[n];
        i->cpu_io_registers[0x19u + n * 2u] = (uint8_t)(i->autojoy_result[n] >> 8);
    }
    i->autojoy_busy = 0u;
    i->autojoy_remaining_clocks = 0u;
}

static void begin_autojoy(TopGear2Recomp *i) {
    if ((i->nmitimen & 1u) == 0u) return;
    i->autojoy_busy = 1u;
    i->autojoy_remaining_clocks = 4224u;
}

static int hdma_reload(TopGear2Recomp *i, unsigned ch) {
    unsigned reg = 0x100u + ch * 0x10u;
    uint8_t bank = i->cpu_io_registers[reg + 4u];
    uint8_t dmap = i->cpu_io_registers[reg];
    uint8_t line, lo, hi;

    if (!tg2_bus_read8(i, ((uint32_t)bank << 16) | i->hdma_table_address[ch], &line)) return 0;
    i->hdma_table_address[ch]++;
    i->hdma_line_counter[ch] = line;

    if (!line) {
        i->hdma_active_mask &= (uint8_t)~(1u << ch);
        i->hdma_completed_mask |= (uint8_t)(1u << ch);
        i->hdma_do_transfer[ch] = 0u;
        return 1;
    }

    i->hdma_do_transfer[ch] = 1u;
    if (dmap & 0x40u) {
        if (!tg2_bus_read8(i, ((uint32_t)bank << 16) | i->hdma_table_address[ch]++, &lo) ||
            !tg2_bus_read8(i, ((uint32_t)bank << 16) | i->hdma_table_address[ch]++, &hi)) return 0;
        i->hdma_indirect_address[ch] = (uint16_t)(lo | ((uint16_t)hi << 8));
    }
    return 1;
}

static int hdma_step_channel(TopGear2Recomp *i, unsigned ch) {
    unsigned reg = 0x100u + ch * 0x10u;
    uint8_t dmap = i->cpu_io_registers[reg];
    uint8_t mode = dmap & 7u;
    uint8_t indirect = (dmap >> 6) & 1u;
    uint8_t direction = (dmap >> 7) & 1u;
    uint8_t bbad = i->cpu_io_registers[reg + 1u];
    uint8_t bank = indirect ? i->cpu_io_registers[reg + 7u] : i->cpu_io_registers[reg + 4u];
    uint8_t n;
    if (direction) return 1;

    if (i->hdma_do_transfer[ch]) {
        uint8_t len = hdma_mode_length(mode);
        for (n = 0; n < len; n++) {
            uint16_t *src = indirect ? &i->hdma_indirect_address[ch] : &i->hdma_table_address[ch];
            uint32_t a = ((uint32_t)bank << 16) | *src;
            uint16_t b = (uint16_t)(0x2100u + ((bbad + hdma_bbus_offset(mode, n)) & 0xFFu));
            uint8_t data;
            if (!tg2_bus_read8(i, a, &data) || !tg2_ppu_write(i, b, data)) return 0;
            (*src)++;
        }
    }
    i->hdma_line_counter[ch]--;
    i->hdma_do_transfer[ch] = (uint8_t)((i->hdma_line_counter[ch] >> 7) & 1u);
    if ((i->hdma_line_counter[ch] & 0x7Fu) == 0u && !hdma_reload(i, ch)) return 0;
    return 1;
}

static int hdma_setup(TopGear2Recomp *i) {
    unsigned ch;
    i->hdma_active_mask = i->hdma_enable_mask;
    i->hdma_completed_mask = 0u;
    if (!i->hdma_active_mask) return 1;

    for (ch = 0; ch < 8; ch++) {
        unsigned reg = 0x100u + ch * 0x10u;
        if ((i->hdma_active_mask & (1u << ch)) == 0u) continue;
        i->hdma_table_address[ch] = (uint16_t)(i->cpu_io_registers[reg + 2u] | ((uint16_t)i->cpu_io_registers[reg + 3u] << 8));
        i->hdma_line_counter[ch] = 0u;
        i->hdma_do_transfer[ch] = 1u;
        if (!hdma_reload(i, ch)) return 0;
        if ((i->hdma_active_mask & (1u << ch)) != 0u && !hdma_step_channel(i, ch)) return 0;
    }
    return 1;
}

static int hdma_line(TopGear2Recomp *i) {
    unsigned ch;
    if (!i->hdma_active_mask) return 1;

    for (ch = 0; ch < 8; ch++) {
        if ((i->hdma_active_mask & (1u << ch)) == 0u) continue;
        if (!hdma_step_channel(i, ch)) return 0;
    }
    return 1;
}

static void irq_poll(TopGear2Recomp *i) {
    uint8_t h = (i->nmitimen >> 4) & 1u, v = (i->nmitimen >> 5) & 1u;
    uint16_t th = (uint16_t)((i->htime_target + 1u) * 4u);
    int level;
    if (!h && !v) {
        i->irq_line = 0u;
        return;
    }
    level = (!v || i->vcounter_line == i->vtime_target) && (!h || i->hcounter_master >= th);
    if (level && !i->irq_line) {
        i->irq_pending = 1u;
        i->irq_flag = 1u;
    }
    i->irq_line = (uint8_t)level;
}

static void line_start(TopGear2Recomp *i) {
    i->line_period = line_period(i);
    i->refresh_done = 0u;
    i->refresh_position = refresh_position(i);
    i->scanline_render_done = 0u;

    if (i->vcounter_line == 0u) {
        hdma_setup(i);
        i->nmi_flag = 0u;
        i->nmi_issued_this_vblank = 0u;
    }

    if (i->vcounter_line == 225u) {
        i->nmi_flag = 1u;
        if (i->nmitimen & 0x80u) {
            i->nmi_pending = 1u;
            i->nmi_issued_this_vblank = 1u;
        }
        begin_autojoy(i);
    }

    if (i->vcounter_line == 240u) {
        tg2_audio_sync(i, i->master_clock);
        i->frame_count++;
        i->frame_ready = 1u;
    }

    irq_poll(i);
}

static void advance_raw(TopGear2Recomp *i, uint32_t clocks) {
    i->master_clock += clocks;
    if (i->autojoy_busy) {
        if (clocks >= i->autojoy_remaining_clocks) complete_autojoy(i);
        else i->autojoy_remaining_clocks -= clocks;
    }
    i->hcounter_master = (uint16_t)(i->hcounter_master + clocks);
}

int tg2_advance_master_clocks(TopGear2Recomp *i, uint32_t clocks) {
    uint32_t remain = clocks;
    if (!i) return 0;

    while (remain) {
        uint16_t h = i->hcounter_master, period = i->line_period;
        uint32_t step = remain;
        uint16_t next = period;
        uint16_t th = (uint16_t)((i->htime_target + 1u) * 4u);

        if (!i->refresh_done && i->refresh_position > h && i->refresh_position < next)
            next = i->refresh_position;
        if (i->vcounter_line < 224u && !i->scanline_render_done && 1080u > h && 1080u < next)
            next = 1080u;
        if (i->vcounter_line < 224u && i->hdma_active_mask && 1104u > h && 1104u < next)
            next = 1104u;
        if ((i->nmitimen & 0x10u) && th > h && th < next)
            next = th;

        if ((uint32_t)(next - h) < step) step = (uint32_t)(next - h);
        if (step) {
            advance_raw(i, step);
            remain -= step;
        }

        h = i->hcounter_master;

        /* DRAM Refresh (40 master clocks) */
        if (!i->refresh_done && h == i->refresh_position) {
            i->refresh_done = 1u;
            advance_raw(i, 40u);
            continue;
        }

        /* Scanline rendering at H=1080 */
        if (i->vcounter_line < 224u && !i->scanline_render_done && h >= 1080u) {
            tg2_render_scanline(i, i->vcounter_line);
            i->scanline_render_done = 1u;
            continue;
        }

        /* HDMA line processing at H=1104 */
        if (i->vcounter_line < 224u && i->hdma_active_mask && h == 1104u) {
            hdma_line(i);
            advance_raw(i, 2u);
            continue;
        }

        irq_poll(i);

        if (h >= period) {
            i->hcounter_master = (uint16_t)(h - period);
            i->vcounter_line++;
            if (i->vcounter_line >= 262u) {
                i->vcounter_line = 0u;
                i->field ^= 1u;
            }
            line_start(i);
            continue;
        }

        if (step == 0u) {
            advance_raw(i, 2u);
            if (remain >= 2u) remain -= 2u;
            else remain = 0u;
        }
    }
    return 1;
}

int tg2_advance_current_instruction(TopGear2Recomp *i, unsigned cycles) {
    if (!cycles) cycles = 3u;
    return tg2_advance_master_clocks(i, cycles * 8u);
}

static void vector_to_interrupt(TopGear2Recomp *i, uint32_t vec_addr) {
    i->in_interrupt = 1u;

    /* Push 65816 interrupt frame: PBR, PC, P */
    if (!i->cpu.e) tg2_push8(i, i->cpu.pbr);
    tg2_push16(i, i->cpu.pc);
    tg2_push8(i, i->cpu.p);
    i->cpu.p |= TG2_P_I;
    i->cpu.p &= (uint8_t)~TG2_P_D;

    uint8_t vlo = 0u, vhi = 0u;
    tg2_bus_read8(i, vec_addr, &vlo);
    tg2_bus_read8(i, vec_addr + 1u, &vhi);
    uint16_t vec = (uint16_t)(vlo | ((uint16_t)vhi << 8));

    if (vec < 0x2000u) {
        uint8_t b0 = 0u, b1 = 0u, b2 = 0u, b3 = 0u;
        tg2_bus_read8(i, (uint32_t)vec, &b0);
        tg2_bus_read8(i, (uint32_t)vec + 1u, &b1);
        tg2_bus_read8(i, (uint32_t)vec + 2u, &b2);
        tg2_bus_read8(i, (uint32_t)vec + 3u, &b3);
        if (b0 == 0x5Cu) { /* JML long trampoline */
            i->cpu.pc = (uint16_t)(b1 | ((uint16_t)b2 << 8));
            i->cpu.pbr = b3;
        } else {
            i->cpu.pbr = 0x00u;
            i->cpu.pc = vec;
        }
    } else {
        i->cpu.pbr = 0x00u;
        i->cpu.pc = vec;
    }

    i->in_interrupt = 0u;
}

int tg2_service_interrupts(TopGear2Recomp *i) {
    if (i->nmi_pending && !i->in_interrupt) {
        i->cpu_waiting = 0u;
        i->nmi_pending = 0u;
        vector_to_interrupt(i, i->cpu.e ? 0x00FFFAu : 0x00FFEAu);
        return 1;
    }

    if (i->irq_pending || i->irq_line) {
        if (i->cpu_waiting) {
            i->cpu_waiting = 0u;
            /* If interrupts are disabled (I=1), WAI wakes up without vectoring */
            if (i->cpu.p & TG2_P_I) {
                return 1;
            }
        }

        if (!i->in_interrupt && (i->cpu.p & TG2_P_I) == 0u) {
            i->irq_pending = 0u;
            vector_to_interrupt(i, i->cpu.e ? 0x00FFFEu : 0x00FFEEu);
            return 1;
        }
    }

    return 1;
}
