// Task de áudio em tempo real (core 1). Dona exclusiva do I2S; os outros
// módulos só enviam comandos.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ha_dsp.h"

typedef enum {
    ENGINE_SILENT,
    ENGINE_AID,  // aparelho auditivo
    ENGINE_TONE, // tons de audiometria / teste
} engine_mode_t;

typedef struct {
    uint32_t proc_us_avg;
    uint32_t proc_us_max;
    float mic_rms_dbfs[2]; // por slot I2S, útil para diagnosticar MIC_SLOT
} engine_stats_t;

void engine_start(void);
void engine_set_mode(engine_mode_t mode);
engine_mode_t engine_get_mode(void);
void engine_set_aid_params(const ha_dsp_params_t *p);

// Tom em um único ouvido; amp_dbfs <= 0.
void engine_play_tone(int ear, float freq, float amp_dbfs, int pulses, float on_ms, float off_ms);
void engine_stop_tone(void);
bool engine_tone_active(void);

void engine_get_stats(engine_stats_t *st);
