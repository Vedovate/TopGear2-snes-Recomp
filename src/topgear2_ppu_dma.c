#include "topgear2_internal.h"
#include <string.h>

static uint8_t vram_increment(uint8_t vmain) {
    static const uint8_t increments[4] = {1u, 32u, 128u, 128u};
    return increments[vmain & 3u];
}

static uint16_t mapped_vram_word_address(const TopGear2Recomp *i) {
    const uint16_t a = i->vram_word_address;
    switch (i->vram_mapping & 3u) {
    case 0u: return a;
    case 1u: return (uint16_t)((a & 0xFF00u) | ((a & 0x001Fu) << 3) | ((a >> 5) & 7u));
    case 2u: return (uint16_t)((a & 0xFE00u) | ((a & 0x003Fu) << 3) | ((a >> 6) & 7u));
    case 3u: return (uint16_t)((a & 0xFC00u) | ((a & 0x007Fu) << 3) | ((a >> 7) & 7u));
    default: return a;
    }
}

static uint16_t vram_word(const TopGear2Recomp *i) {
    const uint32_t address = ((uint32_t)mapped_vram_word_address(i) << 1) & 0xFFFFu;
    return (uint16_t)(i->vram[address] | ((uint16_t)i->vram[(address + 1u) & 0xFFFFu] << 8));
}

static void advance_vram(TopGear2Recomp *i) {
    i->vram_word_address = (uint16_t)(i->vram_word_address + i->vram_increment_words);
}

static void write_oam(TopGear2Recomp *i, uint8_t value) {
    const uint16_t address = (uint16_t)(i->oam_byte_address % 544u);
    if (address < 512u) {
        if ((address & 1u) == 0u) {
            i->oam_latch_low = value;
            i->oam_latch_pending = 1u;
        } else {
            i->oam[(uint16_t)(address - 1u)] = i->oam_latch_pending ? i->oam_latch_low : i->oam[(uint16_t)(address - 1u)];
            i->oam[address] = value;
            i->oam_latch_pending = 0u;
        }
    } else {
        i->oam[address] = value;
    }
    i->oam_byte_address = (uint16_t)((i->oam_byte_address + 1u) % 544u);
}

static void write_cgram(TopGear2Recomp *i, uint8_t value) {
    const uint16_t address = (uint16_t)(i->cgram_byte_address % 512u);
    if ((address & 1u) == 0u) {
        i->cgram_latch_low = value;
        i->cgram_latch_pending = 1u;
    } else {
        i->cgram[(uint16_t)(address - 1u)] = i->cgram_latch_pending ? i->cgram_latch_low : i->cgram[(uint16_t)(address - 1u)];
        i->cgram[address] = (uint8_t)(value & 0x7Fu);
        i->cgram_latch_pending = 0u;
    }
    i->cgram_byte_address = (uint16_t)((i->cgram_byte_address + 1u) % 512u);
}

int tg2_ppu_read(TopGear2Recomp *i, uint16_t address, uint8_t *value) {
    if (!i || !value) return 0;
    if (address < 0x2100u || address > 0x2183u) return 0;
    switch (address) {
    case 0x2134u: { /* MPYL */
        int16_t m7a = (int16_t)i->mode7_a;
        int8_t m7b = (int8_t)i->ppu_registers[0x1Cu];
        int32_t prod = (int32_t)m7a * m7b;
        *value = (uint8_t)prod;
        return 1;
    }
    case 0x2135u: { /* MPYM */
        int16_t m7a = (int16_t)i->mode7_a;
        int8_t m7b = (int8_t)i->ppu_registers[0x1Cu];
        int32_t prod = (int32_t)m7a * m7b;
        *value = (uint8_t)(prod >> 8);
        return 1;
    }
    case 0x2136u: { /* MPYH */
        int16_t m7a = (int16_t)i->mode7_a;
        int8_t m7b = (int8_t)i->ppu_registers[0x1Cu];
        int32_t prod = (int32_t)m7a * m7b;
        *value = (uint8_t)(prod >> 16);
        return 1;
    }
    case 0x2137u: /* SLHV */
        i->ophct_latch = (uint16_t)(i->hcounter_master / 4u);
        i->opvct_latch = i->vcounter_line;
        i->counters_latched = 1u;
        *value = i->cpu_mdr;
        return 1;
    case 0x2138u: /* OAMDATA read */
        *value = i->oam[i->oam_byte_address % 544u];
        i->oam_byte_address = (uint16_t)((i->oam_byte_address + 1u) % 544u);
        return 1;
    case 0x2139u: { /* VMDATAL read */
        uint16_t word = i->vram_read_latch;
        *value = (uint8_t)word;
        if (!i->vram_increment_after_high) {
            i->vram_read_latch = vram_word(i);
            advance_vram(i);
        }
        return 1;
    }
    case 0x213Au: { /* VMDATAH read */
        uint16_t word = i->vram_read_latch;
        *value = (uint8_t)(word >> 8);
        if (i->vram_increment_after_high) {
            i->vram_read_latch = vram_word(i);
            advance_vram(i);
        }
        return 1;
    }
    case 0x213Bu: /* CGDATA read */
        *value = i->cgram[i->cgram_byte_address % 512u];
        i->cgram_byte_address = (uint16_t)((i->cgram_byte_address + 1u) % 512u);
        return 1;
    case 0x213Cu: /* OPHCT */
        if (!i->ophct_high) {
            *value = (uint8_t)(i->ophct_latch & 0xFFu);
            i->ophct_high = 1u;
        } else {
            *value = (uint8_t)((i->cpu_mdr & 0xFEu) | ((i->ophct_latch >> 8) & 1u));
            i->ophct_high = 0u;
        }
        return 1;
    case 0x213Du: /* OPVCT */
        if (!i->opvct_high) {
            *value = (uint8_t)(i->opvct_latch & 0xFFu);
            i->opvct_high = 1u;
        } else {
            *value = (uint8_t)((i->cpu_mdr & 0xFEu) | ((i->opvct_latch >> 8) & 1u));
            i->opvct_high = 0u;
        }
        return 1;
    case 0x213Eu: /* STAT77 */
        *value = 0x01u; /* 5C77 version */
        return 1;
    case 0x213Fu: /* STAT78 */
        *value = (uint8_t)((i->field << 7) | ((i->counters_latched & 1u) << 6) | 0x01u);
        i->ophct_high = 0u;
        i->opvct_high = 0u;
        i->counters_latched = 0u;
        return 1;
    case 0x2180u: { /* WMDATA read */
        *value = i->wram[i->wram_port_address & 0x1FFFFu];
        i->wram_port_address = (i->wram_port_address + 1u) & 0x1FFFFu;
        return 1;
    }
    default:
        *value = i->ppu_registers[address - 0x2100u];
        return 1;
    }
}

int tg2_ppu_write(TopGear2Recomp *i, uint16_t address, uint8_t value) {
    if (!i) return 0;
    if (address < 0x2100u || address > 0x2183u) return 0;
    i->ppu_registers[address - 0x2100u] = value;
    switch (address) {
    case 0x2100u:
        i->forced_blank = (uint8_t)((value >> 7) & 1u);
        i->brightness = (uint8_t)(value & 0x0Fu);
        return 1;
    case 0x2102u:
        i->oam_byte_address = (uint16_t)((i->oam_byte_address & 0x0200u) | ((uint16_t)value << 1));
        i->oam_latch_pending = 0u;
        return 1;
    case 0x2103u:
        i->oam_byte_address = (uint16_t)(((uint16_t)(value & 1u) << 9) | (i->oam_byte_address & 0x01FEu));
        i->oam_priority_rotation = (uint8_t)((value >> 7) & 1u);
        i->oam_latch_pending = 0u;
        return 1;
    case 0x2104u:
        write_oam(i, value);
        return 1;
    case 0x210Du:
        i->mode7_hofs = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        i->bg_scroll[0] = (uint16_t)(((uint16_t)value << 8) | (i->bgofs_ppu1_latch & 0xF8u) | (i->bgofs_ppu2_latch & 7u));
        i->bgofs_ppu1_latch = value;
        i->bgofs_ppu2_latch = value;
        return 1;
    case 0x210Eu:
        i->mode7_vofs = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        i->bg_scroll[1] = (uint16_t)(((uint16_t)value << 8) | i->bgofs_ppu1_latch);
        i->bgofs_ppu1_latch = value;
        return 1;
    case 0x210Fu: case 0x2111u: case 0x2113u: {
        unsigned layer = (unsigned)((address - 0x210Fu) / 2u) + 1u;
        i->bg_scroll[layer * 2u] = (uint16_t)(((uint16_t)value << 8) | (i->bgofs_ppu1_latch & 0xF8u) | (i->bgofs_ppu2_latch & 7u));
        i->bgofs_ppu1_latch = value;
        i->bgofs_ppu2_latch = value;
        return 1;
    }
    case 0x2110u: case 0x2112u: case 0x2114u: {
        unsigned layer = (unsigned)((address - 0x2110u) / 2u) + 1u;
        i->bg_scroll[layer * 2u + 1u] = (uint16_t)(((uint16_t)value << 8) | i->bgofs_ppu1_latch);
        i->bgofs_ppu1_latch = value;
        return 1;
    }
    case 0x211Bu:
        i->mode7_a = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        return 1;
    case 0x211Cu:
        i->mode7_b = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        return 1;
    case 0x211Du:
        i->mode7_c = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        return 1;
    case 0x211Eu:
        i->mode7_d = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        return 1;
    case 0x211Fu:
        i->mode7_x = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        return 1;
    case 0x2120u:
        i->mode7_y = (uint16_t)(((uint16_t)value << 8) | i->mode7_latch);
        i->mode7_latch = value;
        return 1;
    case 0x2132u: {
        uint8_t c = (uint8_t)(value & 0x1Fu);
        if (value & 0x20u) i->ppu_fixed_color = (uint16_t)((i->ppu_fixed_color & 0x7FE0u) | c);
        if (value & 0x40u) i->ppu_fixed_color = (uint16_t)((i->ppu_fixed_color & 0x7C1Fu) | ((uint16_t)c << 5));
        if (value & 0x80u) i->ppu_fixed_color = (uint16_t)((i->ppu_fixed_color & 0x03FFu) | ((uint16_t)c << 10));
        return 1;
    }
    case 0x2115u:
        i->vram_increment_words = vram_increment(value);
        i->vram_increment_after_high = (uint8_t)((value >> 7) & 1u);
        i->vram_mapping = (uint8_t)((value >> 2) & 3u);
        return 1;
    case 0x2116u:
        i->vram_word_address = (uint16_t)((i->vram_word_address & 0xFF00u) | value);
        i->vram_read_latch = vram_word(i);
        return 1;
    case 0x2117u:
        i->vram_word_address = (uint16_t)(((uint16_t)value << 8) | (i->vram_word_address & 0x00FFu));
        i->vram_read_latch = vram_word(i);
        return 1;
    case 0x2118u: {
        const uint32_t a = ((uint32_t)mapped_vram_word_address(i) << 1) & 0xFFFFu;
        i->vram[a] = value;
        if (!i->vram_increment_after_high) advance_vram(i);
        return 1;
    }
    case 0x2119u: {
        const uint32_t a = ((uint32_t)mapped_vram_word_address(i) << 1) & 0xFFFFu;
        i->vram[(a + 1u) & 0xFFFFu] = value;
        if (i->vram_increment_after_high) advance_vram(i);
        return 1;
    }
    case 0x2121u:
        i->cgram_byte_address = (uint16_t)((uint16_t)value << 1);
        i->cgram_latch_pending = 0u;
        return 1;
    case 0x2122u:
        write_cgram(i, value);
        return 1;
    case 0x2180u:
        i->wram[i->wram_port_address & 0x1FFFFu] = value;
        i->wram_port_address = (i->wram_port_address + 1u) & 0x1FFFFu;
        return 1;
    case 0x2181u:
        i->wram_port_address = (i->wram_port_address & 0x1FF00u) | value;
        return 1;
    case 0x2182u:
        i->wram_port_address = (i->wram_port_address & 0x100FFu) | ((uint32_t)value << 8);
        return 1;
    case 0x2183u:
        i->wram_port_address = (i->wram_port_address & 0x0FFFFu) | (((uint32_t)value & 1u) << 16);
        return 1;
    default:
        return 1;
    }
}

static void tg2_dma_start(TopGear2Recomp *i, uint8_t enable_mask) {
    unsigned ch;
    static const uint8_t bbus_offsets[8][4] = {
        {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
        {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1}
    };
    static const uint8_t mode_lengths[8] = {1, 2, 2, 4, 4, 4, 2, 4};

    for (ch = 0; ch < 8; ch++) {
        if ((enable_mask & (1u << ch)) == 0u) continue;
        unsigned reg = 0x100u + ch * 0x10u;
        uint8_t dmap = i->cpu_io_registers[reg];
        uint8_t bbad = i->cpu_io_registers[reg + 1u];
        uint16_t a1t = (uint16_t)(i->cpu_io_registers[reg + 2u] | ((uint16_t)i->cpu_io_registers[reg + 3u] << 8));
        uint8_t a1b  = i->cpu_io_registers[reg + 4u];
        uint16_t das = (uint16_t)(i->cpu_io_registers[reg + 5u] | ((uint16_t)i->cpu_io_registers[reg + 6u] << 8));

        uint8_t mode = dmap & 7u;
        uint8_t fixed = (uint8_t)((dmap >> 3) & 1u);
        uint8_t dec = (uint8_t)((dmap >> 4) & 1u);
        uint8_t from_b = (uint8_t)((dmap >> 7) & 1u);

        uint32_t count = (das == 0u) ? 0x10000u : das;
        uint8_t len = mode_lengths[mode];

        for (uint32_t n = 0; n < count; n++) {
            uint32_t a_addr = ((uint32_t)a1b << 16) | a1t;
            uint16_t b_addr = (uint16_t)(0x2100u + ((bbad + bbus_offsets[mode][n % len]) & 0xFFu));
            uint8_t data = 0;

            if (from_b) {
                tg2_bus_read8(i, b_addr, &data);
                tg2_bus_write8(i, a_addr, data);
            } else {
                tg2_bus_read8(i, a_addr, &data);
                tg2_bus_write8(i, b_addr, data);
            }

            if (!fixed) {
                if (dec) a1t--;
                else a1t++;
            }
        }

        i->cpu_io_registers[reg + 2u] = (uint8_t)a1t;
        i->cpu_io_registers[reg + 3u] = (uint8_t)(a1t >> 8);
        i->cpu_io_registers[reg + 5u] = 0;
        i->cpu_io_registers[reg + 6u] = 0;

        if (!from_b && bbad == 0x04u && count >= 512u) {
            if (i->ws_staging_dirty != 0u) {
                uint16_t cnt = i->ws_staging_count;
                if (cnt > 256u) cnt = 256u;
                if (cnt > 0u) {
                    memcpy(i->ws_sprites, i->ws_staging, (size_t)cnt * sizeof(Tg2WsSprite));
                }
                i->ws_sprite_count = cnt;
                i->ws_staging_count = 0u;
                i->ws_staging_dirty = 0u;
            } else if (!tg2_is_active_race(i)) {
                i->ws_sprite_count = 0u;
            }
        }
    }
}

int tg2_cpu_io_read(TopGear2Recomp *i, uint16_t address, uint8_t *value) {
    if (!i || !value) return 0;
    if (address < 0x4200u || address > 0x43FFu) return 0;
    uint16_t reg = (uint16_t)(address - 0x4200u);

    switch (address) {
    case 0x4210u: { /* RDNMI */
        uint8_t v = (uint8_t)((i->nmi_flag << 7) | 0x02u);
        i->nmi_flag = 0u;
        *value = v;
        return 1;
    }
    case 0x4211u: { /* TIMEUP */
        uint8_t v = (uint8_t)(i->irq_flag << 7);
        i->irq_flag = 0u;
        i->irq_pending = 0u;
        *value = v;
        return 1;
    }
    case 0x4212u: { /* HVBJOY */
        uint8_t in_vblank = (i->vcounter_line >= 225u && i->vcounter_line <= 261u) ? 1u : 0u;
        uint8_t in_hblank = (i->hcounter_master >= 1096u) ? 1u : 0u;
        uint8_t v = (uint8_t)((in_vblank << 7) | (in_hblank << 6) | (i->autojoy_busy ? 1u : 0u));
        *value = v;
        return 1;
    }
    case 0x4214u: case 0x4215u: case 0x4216u: case 0x4217u:
    case 0x4218u: case 0x4219u: case 0x421Au: case 0x421Bu:
    case 0x421Cu: case 0x421Du: case 0x421Eu: case 0x421Fu:
        *value = i->cpu_io_registers[reg];
        return 1;
    default:
        *value = i->cpu_io_registers[reg];
        return 1;
    }
}

int tg2_dma_io_write(TopGear2Recomp *i, uint16_t address, uint8_t value) {
    if (!i) return 0;
    if (address < 0x4200u || address > 0x43FFu) return 0;
    uint16_t reg = (uint16_t)(address - 0x4200u);
    i->cpu_io_registers[reg] = value;

    switch (address) {
    case 0x4200u: /* NMITIMEN */
        i->nmitimen = value;
        if ((value & 0x30u) == 0u) {
            i->irq_flag = 0u;
            i->irq_pending = 0u;
            i->irq_line = 0u;
        }
        return 1;
    case 0x4202u: /* WRMPYA */
        return 1;
    case 0x4203u: { /* WRMPYB */
        uint16_t a = i->cpu_io_registers[0x02u];
        uint16_t prod = (uint16_t)(a * value);
        i->cpu_io_registers[0x16u] = (uint8_t)prod;
        i->cpu_io_registers[0x17u] = (uint8_t)(prod >> 8);
        return 1;
    }
    case 0x4204u: case 0x4205u: /* WRDIVL/H */
        return 1;
    case 0x4206u: { /* WRDIVB */
        uint16_t dividend = (uint16_t)(i->cpu_io_registers[0x04u] | ((uint16_t)i->cpu_io_registers[0x05u] << 8));
        uint8_t divisor = value;
        if (divisor == 0) {
            i->cpu_io_registers[0x14u] = 0xFFu;
            i->cpu_io_registers[0x15u] = 0xFFu;
            i->cpu_io_registers[0x16u] = (uint8_t)dividend;
            i->cpu_io_registers[0x17u] = (uint8_t)(dividend >> 8);
        } else {
            uint16_t quot = (uint16_t)(dividend / divisor);
            uint16_t rem = (uint16_t)(dividend % divisor);
            i->cpu_io_registers[0x14u] = (uint8_t)quot;
            i->cpu_io_registers[0x15u] = (uint8_t)(quot >> 8);
            i->cpu_io_registers[0x16u] = (uint8_t)rem;
            i->cpu_io_registers[0x17u] = (uint8_t)(rem >> 8);
        }
        return 1;
    }
    case 0x4207u:
        i->htime_target = (uint16_t)((i->htime_target & 0x0100u) | value);
        return 1;
    case 0x4208u:
        i->htime_target = (uint16_t)(((uint16_t)(value & 1u) << 8) | (i->htime_target & 0x00FFu));
        return 1;
    case 0x4209u:
        i->vtime_target = (uint16_t)((i->vtime_target & 0x0100u) | value);
        return 1;
    case 0x420Au:
        i->vtime_target = (uint16_t)(((uint16_t)(value & 1u) << 8) | (i->vtime_target & 0x00FFu));
        return 1;
    case 0x420Bu: /* MDMAEN */
        tg2_dma_start(i, value);
        return 1;
    case 0x420Cu: /* HDMAEN */
        i->hdma_enable_mask = value;
        return 1;
    default:
        return 1;
    }
}
