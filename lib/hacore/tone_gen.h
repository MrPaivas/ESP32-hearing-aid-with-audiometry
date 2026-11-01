// Gerador de tom puro pulsado com rampas cos² (evita cliques e espalhamento
// espectral na audiometria).
#pragma once

#include <stdbool.h>

typedef struct {
    float phase, phase_inc, amp;
    int ramp_len, on_len, off_len;
    int pulses, pulse_idx, pos;
    bool active;
} tone_gen_t;

void tone_gen_start(tone_gen_t *t, float fs, float freq, float amp, int pulses, float on_ms, float off_ms,
                    float ramp_ms);
// Termina o pulso atual com rampa de descida (sem clique).
void tone_gen_stop(tone_gen_t *t);
// Escreve n amostras (zeros quando inativo).
void tone_gen_render(tone_gen_t *t, float *out, int n);

static inline bool tone_gen_active(const tone_gen_t *t)
{
    return t->active;
}
