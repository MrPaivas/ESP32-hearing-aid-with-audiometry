#include "dynamics.h"

#include <math.h>

static float time_coef(float rate_hz, float ms)
{
    if (ms <= 0.0f)
        return 0.0f;
    return expf(-1000.0f / (ms * rate_hz));
}

void wdrc_init(wdrc_t *c, float block_rate_hz, float attack_ms, float release_ms)
{
    c->attack_coef = time_coef(block_rate_hz, attack_ms);
    c->release_coef = time_coef(block_rate_hz, release_ms);
    c->gain_db = 0.0f;
    wdrc_set(c, 55.0f, 2.0f, 120.0f);
}

void wdrc_set(wdrc_t *c, float knee_spl, float ratio, float spl_at_0dbfs)
{
    c->knee_spl = knee_spl;
    c->ratio = ratio < 1.0f ? 1.0f : ratio;
    c->spl_at_0dbfs = spl_at_0dbfs;
}

float wdrc_block(wdrc_t *c, const float *in, int n)
{
    float acc = 0.0f;
    for (int i = 0; i < n; i++)
        acc += in[i] * in[i];
    float rms = sqrtf(acc / (float)n);
    float level_spl = 20.0f * log10f(rms + 1e-12f) + c->spl_at_0dbfs;

    float target = 0.0f;
    if (level_spl > c->knee_spl)
        target = -(level_spl - c->knee_spl) * (1.0f - 1.0f / c->ratio);

    // Mais redução = ataque; menos redução = release.
    float k = target < c->gain_db ? c->attack_coef : c->release_coef;
    c->gain_db = target + k * (c->gain_db - target);
    return powf(10.0f, c->gain_db / 20.0f);
}

void limiter_init(limiter_t *l, float fs, float release_ms)
{
    l->release_coef = time_coef(fs, release_ms);
    l->gain = 1.0f;
    l->ceiling = 1.0f;
}

void limiter_set_ceiling_db(limiter_t *l, float ceiling_dbfs)
{
    if (ceiling_dbfs > 0.0f)
        ceiling_dbfs = 0.0f;
    l->ceiling = powf(10.0f, ceiling_dbfs / 20.0f);
}

void limiter_process(limiter_t *l, float *buf, int n)
{
    const float ceil = l->ceiling;
    for (int i = 0; i < n; i++) {
        float a = fabsf(buf[i]);
        float target = a > ceil ? ceil / a : 1.0f;
        if (target < l->gain)
            l->gain = target; // ataque instantâneo
        else
            l->gain = target + l->release_coef * (l->gain - target);
        float y = buf[i] * l->gain;
        // Teto rígido de segurança.
        if (y > ceil)
            y = ceil;
        else if (y < -ceil)
            y = -ceil;
        buf[i] = y;
    }
}
