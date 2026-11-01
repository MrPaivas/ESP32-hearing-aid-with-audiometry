// Cadeia de processamento do aparelho auditivo:
// mic -> passa-altas 100 Hz -> WDRC (ganho comum) -> [por ouvido: ganho
// broadband -> EQ 8 bandas -> limitador MPO] -> L/R
#pragma once

#include "biquad.h"
#include "dynamics.h"
#include "ha_types.h"

typedef struct {
    float eq_gain_db[HA_NUM_EARS][HA_NUM_BANDS]; // ganhos já ajustados por eq_fit
    float broadband_gain_db[HA_NUM_EARS];        // referência acústica + volume
    float limiter_ceiling_dbfs;
    float wdrc_knee_spl;
    float wdrc_ratio;
    float mic_spl_at_0dbfs;
    bool mute;
} ha_dsp_params_t;

typedef struct {
    float fs;
    biquad_t hpf[2]; // 4ª ordem (2 biquads)
    biquad_t eq[HA_NUM_EARS][HA_NUM_BANDS];
    wdrc_t wdrc;
    limiter_t lim[HA_NUM_EARS];
    float gain[HA_NUM_EARS];        // ganho linear atual (suavizado)
    float gain_target[HA_NUM_EARS];
    float gain_smooth;              // coeficiente por amostra
    float wdrc_gain;                // ganho do WDRC no bloco anterior
} ha_dsp_t;

// Após o init o ganho parte de zero e sobe suavemente (sem estalo).
void ha_dsp_init(ha_dsp_t *d, float fs);
void ha_dsp_configure(ha_dsp_t *d, const ha_dsp_params_t *p);
void ha_dsp_process(ha_dsp_t *d, float *in, float *out_l, float *out_r, int n);
