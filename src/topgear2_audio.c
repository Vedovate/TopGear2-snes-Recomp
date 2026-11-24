#include "topgear2_internal.h"
#include "snesrecomp_repo/runner/src/snes/apu.h"
#include "snesrecomp_repo/runner/src/snes/spc.h"
#include "snesrecomp_repo/runner/src/snes/dsp.h"

/* Audio trace stubs required by the pure C APU */
uint64_t g_apu_timer0_total_ticks = 0;
void audio_trace_on_cpu_port_apply(uint8_t p, uint8_t v) { (void)p; (void)v; }
void audio_trace_on_spc_port_read(uint8_t p, uint8_t v) { (void)p; (void)v; }
void audio_trace_on_spc_port_write(uint8_t p, uint8_t v) { (void)p; (void)v; }
void audio_trace_on_sample(int16_t l, int16_t r, int d, uint32_t f) { (void)l; (void)r; (void)d; (void)f; }
void audio_trace_on_reg_write(uint8_t a, uint8_t v) { (void)a; (void)v; }
void audio_trace_on_consume(uint64_t r, uint32_t c, uint32_t a) { (void)r; (void)c; (void)a; }
void* dsp_shadow_create(void) { return NULL; }
void dsp_shadow_free(void* p) { (void)p; }
void dsp_shadow_process(void* p, int16_t l, int16_t r, uint8_t ch) { (void)p; (void)l; (void)r; (void)ch; }

void tg2_audio_init(TopGear2Recomp *instance) {
    if (!instance) return;
    instance->apu = apu_init();
    if (instance->apu) {
        apu_reset(instance->apu);
    }
    instance->last_apu_master_clock = 0u;
    instance->apu_catchup_cycles = 0.0;
}

void tg2_audio_reset(TopGear2Recomp *instance) {
    if (!instance) return;
    if (instance->apu) {
        apu_reset(instance->apu);
    }
    instance->last_apu_master_clock = 0u;
    instance->apu_catchup_cycles = 0.0;
}

void tg2_audio_free(TopGear2Recomp *instance) {
    if (!instance) return;
    if (instance->apu) {
        apu_free(instance->apu);
        instance->apu = NULL;
    }
}

void tg2_audio_sync(TopGear2Recomp *instance, uint64_t target_clock) {
    if (!instance || !instance->apu) return;
    if (target_clock <= instance->last_apu_master_clock) return;

    /* SNES master clock to SPC cycle ratio:
     * 32040.0 * 32.0 / (1364.0 * 262.0 * 60.0988) ≈ 1 / 20.91278 master clocks per APU cycle */
    static const double kApuCyclesPerMasterClock = (32040.0 * 32.0) / (1364.0 * 262.0 * 60.0988);

    uint64_t delta = target_clock - instance->last_apu_master_clock;
    instance->last_apu_master_clock = target_clock;
    instance->apu_catchup_cycles += (double)delta * kApuCyclesPerMasterClock;

    int cycles_to_run = (int)instance->apu_catchup_cycles;
    if (cycles_to_run > 0) {
        if (cycles_to_run > 10000) cycles_to_run = 10000;
        for (int c = 0; c < cycles_to_run; c++) {
            apu_cycle(instance->apu);
        }
        instance->apu_catchup_cycles -= (double)cycles_to_run;
        if (instance->apu_catchup_cycles < 0.0) instance->apu_catchup_cycles = 0.0;
    }
}

uint8_t tg2_audio_cpu_read(TopGear2Recomp *instance, uint8_t port) {
    if (!instance || !instance->apu) return 0;
    tg2_audio_sync(instance, instance->master_clock);
    return instance->apu->outPorts[port & 3u];
}

void tg2_audio_cpu_write(TopGear2Recomp *instance, uint8_t port, uint8_t value) {
    if (!instance || !instance->apu) return;
    tg2_audio_sync(instance, instance->master_clock);
    apu_writePortNow(instance->apu, (uint8_t)(port & 3u), value);
}

size_t tg2_audio_extract_samples(TopGear2Recomp *instance, int16_t *buffer, size_t max_samples) {
    if (!instance || !instance->apu || !buffer || max_samples == 0) return 0;
    return (size_t)dsp_getSamples(instance->apu->dsp, buffer, (int)(max_samples / 2));
}
