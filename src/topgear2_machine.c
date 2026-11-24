#include "topgear2_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tg2_set_flag(TopGear2Recomp *instance, uint8_t flag, int enabled) {
    if (enabled) instance->cpu.p |= flag;
    else instance->cpu.p &= (uint8_t)~flag;
}

int tg2_flag(const TopGear2Recomp *instance, uint8_t flag) {
    return (instance->cpu.p & flag) != 0u;
}

void tg2_set_nz8(TopGear2Recomp *instance, uint8_t value) {
    instance->cpu.p &= (uint8_t)~(TG2_P_N | TG2_P_Z);
    if (value == 0u) instance->cpu.p |= TG2_P_Z;
    if ((value & 0x80u) != 0u) instance->cpu.p |= TG2_P_N;
}

void tg2_set_nz16(TopGear2Recomp *instance, uint16_t value) {
    instance->cpu.p &= (uint8_t)~(TG2_P_N | TG2_P_Z);
    if (value == 0u) instance->cpu.p |= TG2_P_Z;
    if ((value & 0x8000u) != 0u) instance->cpu.p |= TG2_P_N;
}

uint8_t tg2_acc8(const TopGear2Recomp *instance) {
    return (uint8_t)instance->cpu.a;
}

uint16_t tg2_acc16(const TopGear2Recomp *instance) {
    return instance->cpu.a;
}

void tg2_set_acc8(TopGear2Recomp *instance, uint8_t value) {
    instance->cpu.a = (uint16_t)((instance->cpu.a & 0xFF00u) | value);
}

void tg2_set_acc16(TopGear2Recomp *instance, uint16_t value) {
    instance->cpu.a = value;
}

void tg2_set_index8(TopGear2Recomp *instance, uint16_t *reg, uint8_t value) {
    (void)instance; *reg = value;
}

void tg2_set_index16(TopGear2Recomp *instance, uint16_t *reg, uint16_t value) {
    (void)instance; *reg = value;
}

void tg2_compare8(TopGear2Recomp *instance, uint8_t left, uint8_t right) {
    uint8_t result = (uint8_t)(left - right);
    tg2_set_flag(instance, TG2_P_C, left >= right);
    tg2_set_nz8(instance, result);
}

void tg2_compare16(TopGear2Recomp *instance, uint16_t left, uint16_t right) {
    uint16_t result = (uint16_t)(left - right);
    tg2_set_flag(instance, TG2_P_C, left >= right);
    tg2_set_nz16(instance, result);
}

void tg2_adc8(TopGear2Recomp *instance, uint8_t value) {
    uint8_t left = tg2_acc8(instance);
    unsigned carry = tg2_flag(instance, TG2_P_C) ? 1u : 0u;
    int result = 0;
    if (tg2_flag(instance, TG2_P_D)) {
        result = (left & 0xf) + (value & 0xf) + carry;
        if (result > 0x9) result = ((result + 0x6) & 0xf) + 0x10;
        result = (left & 0xf0) + (value & 0xf0) + result;
    } else {
        result = (int)left + (int)value + (int)carry;
    }
    tg2_set_flag(instance, TG2_P_V, ((left & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80)));
    if (tg2_flag(instance, TG2_P_D) && result > 0x9f) result += 0x60;
    tg2_set_flag(instance, TG2_P_C, result > 0xff);
    uint8_t out = (uint8_t)result;
    tg2_set_acc8(instance, out);
    tg2_set_nz8(instance, out);
}

void tg2_adc16(TopGear2Recomp *instance, uint16_t value) {
    uint16_t left = instance->cpu.a;
    unsigned carry = tg2_flag(instance, TG2_P_C) ? 1u : 0u;
    int result = 0;
    if (tg2_flag(instance, TG2_P_D)) {
        result = (left & 0xf) + (value & 0xf) + carry;
        if (result > 0x9) result = ((result + 0x6) & 0xf) + 0x10;
        result = (left & 0xf0) + (value & 0xf0) + result;
        if (result > 0x9f) result = ((result + 0x60) & 0xff) + 0x100;
        result = (left & 0xf00) + (value & 0xf00) + result;
        if (result > 0x9ff) result = ((result + 0x600) & 0xfff) + 0x1000;
        result = (left & 0xf000) + (value & 0xf000) + result;
    } else {
        result = (int)left + (int)value + (int)carry;
    }
    tg2_set_flag(instance, TG2_P_V, ((left & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000)));
    if (tg2_flag(instance, TG2_P_D) && result > 0x9fff) result += 0x6000;
    tg2_set_flag(instance, TG2_P_C, (uint32_t)result > 0xffffu);
    uint16_t out = (uint16_t)result;
    instance->cpu.a = out;
    tg2_set_nz16(instance, out);
}

void tg2_sbc8(TopGear2Recomp *instance, uint8_t value_in) {
    uint8_t left = tg2_acc8(instance);
    uint8_t value = value_in ^ 0xff;
    unsigned carry = tg2_flag(instance, TG2_P_C) ? 1u : 0u;
    int result = 0;
    if (tg2_flag(instance, TG2_P_D)) {
        result = (left & 0xf) + (value & 0xf) + carry;
        if (result < 0x10) result = (result - 0x6) & ((result - 0x6 < 0) ? 0xf : 0x1f);
        result = (left & 0xf0) + (value & 0xf0) + result;
    } else {
        result = (int)left + (int)value + (int)carry;
    }
    tg2_set_flag(instance, TG2_P_V, ((left & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80)));
    if (tg2_flag(instance, TG2_P_D) && result < 0x100) result -= 0x60;
    tg2_set_flag(instance, TG2_P_C, result > 0xff);
    uint8_t out = (uint8_t)result;
    tg2_set_acc8(instance, out);
    tg2_set_nz8(instance, out);
}

void tg2_sbc16(TopGear2Recomp *instance, uint16_t value_in) {
    uint16_t left = instance->cpu.a;
    uint16_t value = value_in ^ 0xffff;
    unsigned carry = tg2_flag(instance, TG2_P_C) ? 1u : 0u;
    int result = 0;
    if (tg2_flag(instance, TG2_P_D)) {
        result = (left & 0xf) + (value & 0xf) + carry;
        if (result < 0x10) result = (result - 0x6) & ((result - 0x6 < 0) ? 0xf : 0x1f);
        result = (left & 0xf0) + (value & 0xf0) + result;
        if (result < 0x100) result = (result - 0x60) & ((result - 0x60 < 0) ? 0xff : 0x1ff);
        result = (left & 0xf00) + (value & 0xf00) + result;
        if (result < 0x1000) result = (result - 0x600) & ((result - 0x600 < 0) ? 0xfff : 0x1fff);
        result = (left & 0xf000) + (value & 0xf000) + result;
    } else {
        result = (int)left + (int)value + (int)carry;
    }
    tg2_set_flag(instance, TG2_P_V, ((left & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000)));
    if (tg2_flag(instance, TG2_P_D) && result < 0x10000) result -= 0x6000;
    tg2_set_flag(instance, TG2_P_C, (uint32_t)result > 0xffffu);
    uint16_t out = (uint16_t)result;
    instance->cpu.a = out;
    tg2_set_nz16(instance, out);
}

/* Memory mapping helpers */
static int wram_offset(uint32_t address, uint32_t *offset) {
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;
    if (bank == 0x7Eu || bank == 0x7Fu) {
        *offset = ((uint32_t)(bank - 0x7Eu) << 16) | local;
        return 1;
    }
    if ((bank <= 0x3Fu || (bank >= 0x80u && bank <= 0xBFu)) && local < 0x2000u) {
        *offset = local;
        return 1;
    }
    return 0;
}

static int lorom_offset(const TopGear2Recomp *instance, uint32_t address, uint32_t *offset) {
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;
    uint8_t low_bank;
    if (bank == 0x7Eu || bank == 0x7Fu) return 0;
    low_bank = (uint8_t)(bank & 0x7Fu);
    if (low_bank < 0x40u && local < 0x8000u) return 0;
    if (instance->rom_size == 0) return 0;
    *offset = (((uint32_t)(low_bank & 0x1Fu) << 15) | (local & 0x7FFFu)) % instance->rom_size;
    return 1;
}

static uint8_t controller_serial_bit(TopGear2Recomp *i, unsigned port, int shift) {
    uint8_t bit = (uint8_t)((i->controller_shift[port] >> 15) & 1u);
    if (shift && !i->controller_strobe) {
        i->controller_shift[port] = (uint16_t)(i->controller_shift[port] << 1);
    }
    return bit;
}

int tg2_bus_read8(TopGear2Recomp *instance, uint32_t address, uint8_t *value) {
    uint32_t offset;
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;
    if (!instance || !value) return 0;

    if (wram_offset(address, &offset)) {
        *value = instance->wram[offset];
        instance->cpu_mdr = *value;
        return 1;
    }

    if (bank <= 0x3Fu || (bank >= 0x80u && bank <= 0xBFu)) {
        if (local == 0x4016u || local == 0x4017u) {
            uint8_t data = instance->cpu_mdr;
            unsigned port = (local == 0x4016u) ? 0u : 1u;
            uint8_t serial = controller_serial_bit(instance, port, 1);
            if (local == 0x4016u) data = (uint8_t)((data & 0xFCu) | serial);
            else data = (uint8_t)((data & 0xE0u) | 0x1Cu | serial);
            *value = data;
            return 1;
        }

        if (local >= 0x2100u && local <= 0x2183u) {
            if (local >= 0x2140u && local <= 0x217Fu) {
                unsigned port = (unsigned)((local - 0x2140u) & 3u);
                *value = tg2_audio_cpu_read(instance, (uint8_t)port);
                instance->cpu_mdr = *value;
                return 1;
            }
            if (!tg2_ppu_read(instance, local, value)) return 0;
            instance->cpu_mdr = *value;
            return 1;
        }

        if (local >= 0x4200u && local <= 0x43FFu) {
            if (!tg2_cpu_io_read(instance, local, value)) return 0;
            return 1;
        }
    }

    if (lorom_offset(instance, address, &offset)) {
        *value = instance->rom[offset];
        instance->cpu_mdr = *value;
        return 1;
    }

    *value = instance->cpu_mdr;
    return 1;
}

int tg2_bus_read16(TopGear2Recomp *instance, uint32_t address, uint16_t *value) {
    uint8_t lo = 0u, hi = 0u;
    uint32_t bank = address & 0xFF0000u;
    uint16_t local = (uint16_t)address;
    if (!tg2_bus_read8(instance, bank | local, &lo)) return 0;
    if (!tg2_bus_read8(instance, bank | (uint16_t)(local + 1u), &hi)) return 0;
    *value = (uint16_t)(lo | ((uint16_t)hi << 8));
    return 1;
}

int tg2_bus_read24(TopGear2Recomp *instance, uint32_t address, uint32_t *value) {
    uint8_t b0 = 0u, b1 = 0u, b2 = 0u;
    uint32_t bank = address & 0xFF0000u;
    uint16_t local = (uint16_t)address;
    if (!tg2_bus_read8(instance, bank | local, &b0)) return 0;
    if (!tg2_bus_read8(instance, bank | (uint16_t)(local + 1u), &b1)) return 0;
    if (!tg2_bus_read8(instance, bank | (uint16_t)(local + 2u), &b2)) return 0;
    *value = (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16);
    return 1;
}

int tg2_bus_write8(TopGear2Recomp *instance, uint32_t address, uint8_t value) {
    uint32_t offset;
    uint8_t bank = (uint8_t)(address >> 16);
    uint16_t local = (uint16_t)address;
    if (!instance) return 0;

    instance->cpu_mdr = value;

    if (wram_offset(address, &offset)) {
        instance->wram[offset] = value;
        return 1;
    }

    if (bank <= 0x3Fu || (bank >= 0x80u && bank <= 0xBFu)) {
        if (local == 0x4016u) {
            uint8_t strobe = (uint8_t)(value & 1u);
            instance->controller_strobe = strobe;
            if (strobe) {
                instance->controller_shift[0] = instance->controller_state[0];
                instance->controller_shift[1] = instance->controller_state[1];
            }
            return 1;
        }
        if (local == 0x4017u) return 1;

        if (local >= 0x2100u && local <= 0x2183u) {
            if (local >= 0x2140u && local <= 0x217Fu) {
                unsigned port = (unsigned)((local - 0x2140u) & 3u);
                tg2_audio_cpu_write(instance, (uint8_t)port, value);
                return 1;
            }
            return tg2_ppu_write(instance, local, value);
        }

        if (local >= 0x4200u && local <= 0x43FFu) {
            return tg2_dma_io_write(instance, local, value);
        }
    }

    /* Cartridge ROM writes (ignored / SRAM if present) */
    return 1;
}

int tg2_bus_write16(TopGear2Recomp *instance, uint32_t address, uint16_t value) {
    uint32_t bank = address & 0xFF0000u;
    uint16_t local = (uint16_t)address;
    if (!tg2_bus_write8(instance, bank | (uint16_t)(local + 1u), (uint8_t)(value >> 8))) return 0;
    return tg2_bus_write8(instance, bank | local, (uint8_t)value);
}

int tg2_bus_store16(TopGear2Recomp *instance, uint32_t address, uint16_t value) {
    uint32_t bank = address & 0xFF0000u;
    uint16_t local = (uint16_t)address;
    if (!tg2_bus_write8(instance, bank | local, (uint8_t)value)) return 0;
    return tg2_bus_write8(instance, bank | (uint16_t)(local + 1u), (uint8_t)(value >> 8));
}

int tg2_push8(TopGear2Recomp *instance, uint8_t value) {
    uint16_t address = instance->cpu.e ? (uint16_t)(0x0100u | (instance->cpu.s & 0xFFu)) : instance->cpu.s;
    if (!tg2_bus_write8(instance, address, value)) return 0;
    instance->cpu.s = (uint16_t)(instance->cpu.s - 1u);
    if (instance->cpu.e) instance->cpu.s = (uint16_t)(0x0100u | (instance->cpu.s & 0xFFu));
    return 1;
}

int tg2_push16(TopGear2Recomp *instance, uint16_t value) {
    return tg2_push8(instance, (uint8_t)(value >> 8)) && tg2_push8(instance, (uint8_t)value);
}

int tg2_pull8(TopGear2Recomp *instance, uint8_t *value) {
    instance->cpu.s = (uint16_t)(instance->cpu.s + 1u);
    if (instance->cpu.e) instance->cpu.s = (uint16_t)(0x0100u | (instance->cpu.s & 0xFFu));
    return tg2_bus_read8(instance, instance->cpu.s, value);
}

int tg2_pull16(TopGear2Recomp *instance, uint16_t *value) {
    uint8_t lo = 0u, hi = 0u;
    if (!tg2_pull8(instance, &lo) || !tg2_pull8(instance, &hi)) return 0;
    *value = (uint16_t)(lo | ((uint16_t)hi << 8));
    return 1;
}
