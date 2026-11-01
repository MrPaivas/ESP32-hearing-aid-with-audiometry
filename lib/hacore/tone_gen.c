#include "tone_gen.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TWO_PI ((float)(2.0 * M_PI))

void tone_gen_start(tone_gen_t *t, float fs, float freq, float amp, int pulses, float on_ms, float off_ms,
                    float ramp_ms)
{
    t->phase = 0.0f;
    t->phase_inc = TWO_PI * freq / fs;
    t->amp = amp;
    t->on_len = (int)(on_ms * fs / 1000.0f);
    t->off_len = (int)(off_ms * fs / 1000.0f);
    t->ramp_len = (int)(ramp_ms * fs / 1000.0f);
    if (t->ramp_len * 2 > t->on_len)
        t->ramp_len = t->on_len / 2;
    t->pulses = pulses < 1 ? 1 : pulses;
    t->pulse_idx = 0;
    t->pos = 0;
    t->active = t->on_len > 0;
}

void tone_gen_stop(tone_gen_t *t)
{
    if (!t->active)
        return;
    t->pulses = t->pulse_idx + 1;
    if (t->pos >= t->on_len) {
        t->active = false; // já estava no silêncio entre pulsos
        return;
    }
    int ramp_start = t->on_len - t->ramp_len;
    if (t->pos < ramp_start) {
        // Salta para o ponto da rampa de descida com a mesma amplitude atual.
        int into_ramp = t->pos < t->ramp_len ? t->ramp_len - t->pos : 0;
        t->pos = ramp_start + into_ramp;
    }
}

static float envelope(const tone_gen_t *t)
{
    int p = t->pos;
    if (t->ramp_len <= 0)
        return 1.0f;
    float x;
    if (p < t->ramp_len)
        x = (float)p / (float)t->ramp_len;
    else if (p >= t->on_len - t->ramp_len)
        x = (float)(t->on_len - p) / (float)t->ramp_len;
    else
        return 1.0f;
    float s = sinf(0.5f * (float)M_PI * x);
    return s * s;
}

void tone_gen_render(tone_gen_t *t, float *out, int n)
{
    for (int i = 0; i < n; i++) {
        if (!t->active) {
            out[i] = 0.0f;
            continue;
        }
        if (t->pos < t->on_len) {
            out[i] = t->amp * envelope(t) * sinf(t->phase);
            t->phase += t->phase_inc;
            if (t->phase >= TWO_PI)
                t->phase -= TWO_PI;
        } else {
            out[i] = 0.0f;
        }
        t->pos++;
        bool last = t->pulse_idx + 1 >= t->pulses;
        if (t->pos >= t->on_len + (last ? 0 : t->off_len)) {
            t->pos = 0;
            if (++t->pulse_idx >= t->pulses)
                t->active = false;
        }
    }
}
