#include "ha_dsp.h"

#include <math.h>
#include <string.h>

#include "eq_fit.h"

#define HPF_HZ        100.0f
#define GAIN_RAMP_MS  30.0f
#define WDRC_ATTACK   5.0f
#define WDRC_RELEASE  50.0f
#define LIM_RELEASE   50.0f

void ha_dsp_init(ha_dsp_t *d, float fs)
{
    memset(d, 0, sizeof(*d));
    d->fs = fs;
    // Butterworth de 4ª ordem = dois biquads com Q 0,541 e 1,307.
    biquad_design_highpass(&d->hpf[0], fs, HPF_HZ, 0.5412f);
    biquad_design_highpass(&d->hpf[1], fs, HPF_HZ, 1.3066f);
    for (int e = 0; e < HA_NUM_EARS; e++) {
        for (int b = 0; b < HA_NUM_BANDS; b++)
            biquad_set_identity(&d->eq[e][b]);
        limiter_init(&d->lim[e], fs, LIM_RELEASE);
        d->gain[e] = 0.0f;
        d->gain_target[e] = 0.0f;
    }
    wdrc_init(&d->wdrc, fs / HA_BLOCK_SIZE, WDRC_ATTACK, WDRC_RELEASE);
    d->wdrc_gain = 1.0f;
    d->gain_smooth = expf(-1000.0f / (GAIN_RAMP_MS * fs));
}

void ha_dsp_configure(ha_dsp_t *d, const ha_dsp_params_t *p)
{
    for (int e = 0; e < HA_NUM_EARS; e++) {
        eq_design_bank(d->eq[e], d->fs, p->eq_gain_db[e]);
        limiter_set_ceiling_db(&d->lim[e], p->limiter_ceiling_dbfs);
        d->gain_target[e] = p->mute ? 0.0f : powf(10.0f, p->broadband_gain_db[e] / 20.0f);
    }
    wdrc_set(&d->wdrc, p->wdrc_knee_spl, p->wdrc_ratio, p->mic_spl_at_0dbfs);
}

void ha_dsp_process(ha_dsp_t *d, float *in, float *out_l, float *out_r, int n)
{
    biquad_process(&d->hpf[0], in, n);
    biquad_process(&d->hpf[1], in, n);

    // Ganho do WDRC interpolado ao longo do bloco para evitar "zipper noise".
    float g0 = d->wdrc_gain;
    float g1 = wdrc_block(&d->wdrc, in, n);
    d->wdrc_gain = g1;
    float dg = (g1 - g0) / (float)n;

    float *outs[HA_NUM_EARS] = {out_l, out_r};
    for (int e = 0; e < HA_NUM_EARS; e++) {
        float *out = outs[e];
        float g = d->gain[e];
        const float tgt = d->gain_target[e];
        const float k = d->gain_smooth;
        for (int i = 0; i < n; i++) {
            g = tgt + k * (g - tgt);
            out[i] = in[i] * g * (g0 + dg * (float)i);
        }
        d->gain[e] = g;
        for (int b = 0; b < HA_NUM_BANDS; b++)
            biquad_process(&d->eq[e][b], out, n);
        limiter_process(&d->lim[e], out, n);
    }
}
