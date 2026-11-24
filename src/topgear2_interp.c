#include "topgear2_internal.h"
#include "snesrecomp_repo/runner/src/snes/interp816.h"

static uint8_t interp_read(void *mem, uint32_t adr) {
    TopGear2Recomp *instance = (TopGear2Recomp*)mem;
    uint8_t val = 0;
    tg2_bus_read8(instance, adr, &val);
    return val;
}

static void interp_write(void *mem, uint32_t adr, uint8_t val) {
    TopGear2Recomp *instance = (TopGear2Recomp*)mem;
    tg2_bus_write8(instance, adr, val);
}

int tg2_fallback_step(TopGear2Recomp *instance) {
    if (!instance) return 0;

    if (!instance->interp) {
        instance->interp = interp816_init(instance, interp_read, interp_write);
        if (!instance->interp) return 0;
    }

    Interp816 *it = instance->interp;

    /* Sync CPU state into interpreter */
    it->a  = instance->cpu.a;
    it->x  = instance->cpu.x;
    it->y  = instance->cpu.y;
    it->sp = instance->cpu.s;
    it->dp = instance->cpu.d;
    it->pc = instance->cpu.pc;
    it->k  = instance->cpu.pbr;
    it->db = instance->cpu.dbr;
    it->e  = instance->cpu.e != 0;

    it->c  = (instance->cpu.p & TG2_P_C) != 0;
    it->z  = (instance->cpu.p & TG2_P_Z) != 0;
    it->i  = (instance->cpu.p & TG2_P_I) != 0;
    it->d  = (instance->cpu.p & TG2_P_D) != 0;
    it->xf = (instance->cpu.p & TG2_P_X) != 0;
    it->mf = (instance->cpu.p & TG2_P_M) != 0;
    it->v  = (instance->cpu.p & TG2_P_V) != 0;
    it->n  = (instance->cpu.p & TG2_P_N) != 0;

    it->waiting = false;
    it->stopped = false;

    int cyc = interp816_runOpcode(it);
    (void)cyc;

    if (it->waiting) {
        instance->cpu_waiting = 1u;
    }

    /* Sync state back from interpreter to CPU */
    instance->cpu.a   = it->a;
    instance->cpu.x   = it->x;
    instance->cpu.y   = it->y;
    instance->cpu.s   = it->sp;
    instance->cpu.d   = it->dp;
    instance->cpu.pc  = it->pc;
    instance->cpu.pbr = it->k;
    instance->cpu.dbr = it->db;
    instance->cpu.e   = it->e ? 1 : 0;

    instance->cpu.p = 0;
    if (it->c)  instance->cpu.p |= TG2_P_C;
    if (it->z)  instance->cpu.p |= TG2_P_Z;
    if (it->i)  instance->cpu.p |= TG2_P_I;
    if (it->d)  instance->cpu.p |= TG2_P_D;
    if (it->xf) instance->cpu.p |= TG2_P_X;
    if (it->mf) instance->cpu.p |= TG2_P_M;
    if (it->v)  instance->cpu.p |= TG2_P_V;
    if (it->n)  instance->cpu.p |= TG2_P_N;

    instance->instruction_count++;
    return 1;
}

static inline int ws_is_16_9_race(const TopGear2Recomp *instance) {
    return instance->aspect_mode == TG2_ASPECT_16_9 && tg2_is_active_race(instance);
}

static inline int ws_finish_step(TopGear2Recomp *instance, uint16_t next_pc) {
    instance->cpu.pc = next_pc;
    instance->instruction_count++;
    return 1;
}

static inline int ws_capture_tile_or_fallback(TopGear2Recomp *instance, uint8_t tile_size, uint16_t target_pc) {
    if (tg2_ws_try_capture_tile(instance, tile_size)) {
        return ws_finish_step(instance, target_pc);
    }
    return -1;
}

static inline int ws_read_dp_adjusted(TopGear2Recomp *instance, uint8_t dp_off,
                                      uint16_t orig_val, uint16_t wide_val, uint16_t *out) {
    if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + dp_off) & 0xFFFFu), out)) {
        return 0;
    }
    if (ws_is_16_9_race(instance) && *out == orig_val) {
        *out = wide_val;
    }
    return 1;
}

int tg2_interp_step_with_hooks(TopGear2Recomp *instance) {
    if (!instance) return 0;

    if (instance->cpu.pbr == 0x8Bu && !instance->cpu.e && (instance->cpu.p & (TG2_P_M | TG2_P_X)) == 0u) {
        uint16_t word = 0u;
        switch (instance->cpu.pc) {
        /* WIDESCREEN_PREPEND hooks (bank $8B, e=0, m=0, x=0) */
        case 0x858Fu:
            instance->ws_staging_count = 0u;
            instance->ws_staging_dirty = 0u;
            break;
        case 0xA7C1u:
            if (ws_is_16_9_race(instance)) {
                uint16_t d34 = 0u;
                if (!tg2_bus_read16(instance, (uint32_t)((instance->cpu.d + 0x34u) & 0xFFFFu), &d34)) {
                    return 0;
                }
                if (d34 == 0u) {
                    int16_t sx = (int16_t)tg2_acc16(instance);
                    return ws_finish_step(instance, (sx < -71) ? 0xA7D8u : 0xA7C3u);
                }
            }
            break;
        case 0xA873u: {
            int res = ws_capture_tile_or_fallback(instance, 16u, 0xA8B9u);
            if (res >= 0) return res;
            break;
        }
        case 0xA8F2u: {
            int res = ws_capture_tile_or_fallback(instance, 8u, 0xA938u);
            if (res >= 0) return res;
            break;
        }
        case 0xABB6u:
        case 0xAC0Eu: {
            int res = ws_capture_tile_or_fallback(instance, 16u, 0xAC30u);
            if (res >= 0) return res;
            break;
        }
        case 0xAC77u:
        case 0xACCFu: {
            int res = ws_capture_tile_or_fallback(instance, 8u, 0xACF1u);
            if (res >= 0) return res;
            break;
        }
        case 0xD7E7u:
            instance->ws_staging_dirty = 1u;
            break;

        /* WIDESCREEN_REPLACE hooks (bank $8B, e=0, m=0, x=0) */
        case 0xA7C9u:
        case 0xAAF5u: {
            uint16_t next_pc = (instance->cpu.pc == 0xA7C9u) ? 0xA7CBu : 0xAAF7u;
            if (!ws_read_dp_adjusted(instance, 0x36u, 0x0100u, 0x0147u, &word)) return 0;
            tg2_compare16(instance, tg2_acc16(instance), word);
            return ws_finish_step(instance, next_pc);
        }
        case 0xA80Cu:
        case 0xAB23u: {
            uint16_t next_pc = (instance->cpu.pc == 0xA80Cu) ? 0xA80Eu : 0xAB25u;
            if (!ws_read_dp_adjusted(instance, 0x34u, 0u, 0xFFB9u, &word)) return 0;
            tg2_sbc16(instance, word);
            return ws_finish_step(instance, next_pc);
        }
        case 0xA827u:
        case 0xAB3Eu: {
            uint16_t next_pc = (instance->cpu.pc == 0xA827u) ? 0xA829u : 0xAB40u;
            if (!ws_read_dp_adjusted(instance, 0x36u, 0x0100u, 0x0147u, &word)) return 0;
            tg2_set_acc16(instance, word);
            tg2_set_nz16(instance, word);
            return ws_finish_step(instance, next_pc);
        }
        case 0xA9C9u:
        case 0xAA5Fu: {
            uint16_t next_pc = (instance->cpu.pc == 0xA9C9u) ? 0xA9CCu : 0xAA62u;
            word = ws_is_16_9_race(instance) ? 0x0147u : 0x0100u;
            tg2_compare16(instance, tg2_acc16(instance), word);
            return ws_finish_step(instance, next_pc);
        }
        case 0xA9D4u:
        case 0xAA6Au: {
            uint16_t taken = (instance->cpu.pc == 0xA9D4u) ? 0xAA30u : 0xAAC6u;
            uint16_t fall  = (instance->cpu.pc == 0xA9D4u) ? 0xA9D6u : 0xAA6Cu;
            int cond = ws_is_16_9_race(instance) ? ((int16_t)tg2_acc16(instance) < -71)
                                                 : tg2_flag(instance, TG2_P_N);
            return ws_finish_step(instance, cond ? taken : fall);
        }
        case 0xA9F5u:
        case 0xAA8Bu: {
            uint16_t taken = (instance->cpu.pc == 0xA9F5u) ? 0xAA06u : 0xAA9Cu;
            uint16_t fall  = (instance->cpu.pc == 0xA9F5u) ? 0xA9F7u : 0xAA8Du;
            int cond = ws_is_16_9_race(instance) ? ((int16_t)tg2_acc16(instance) >= -71)
                                                 : !tg2_flag(instance, TG2_P_N);
            return ws_finish_step(instance, cond ? taken : fall);
        }
        case 0xAA0Eu:
        case 0xAAA4u: {
            uint16_t next_pc = (instance->cpu.pc == 0xAA0Eu) ? 0xAA11u : 0xAAA7u;
            word = ws_is_16_9_race(instance) ? 0x0147u : 0x0100u;
            tg2_set_acc16(instance, word);
            tg2_set_nz16(instance, word);
            return ws_finish_step(instance, next_pc);
        }
        case 0xAAFFu:
            if (!ws_read_dp_adjusted(instance, 0x34u, 0u, 0xFFB9u, &word)) return 0;
            tg2_compare16(instance, tg2_acc16(instance), word);
            return ws_finish_step(instance, 0xAB01u);
        default:
            break;
        }
    }

    return tg2_fallback_step(instance);
}

int tg2_semantic_step(TopGear2Recomp *instance) {
    if (!instance) return 0;
    if (!tg2_service_interrupts(instance)) return 0;
    if (instance->cpu_waiting) {
        tg2_advance_current_instruction(instance, 3u);
        return 1;
    }
    if (!tg2_generated_static_step(instance)) return 0;
    tg2_advance_current_instruction(instance, 3u);
    return 1;
}
