// Biquad IIR (forma direta II transposta) com projetos do RBJ Audio EQ Cookbook.
#pragma once

typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} biquad_t;

void biquad_set_identity(biquad_t *bq);
void biquad_reset(biquad_t *bq);

// Os projetos só alteram coeficientes; o estado é preservado para permitir
// atualização em tempo real sem estalos grandes.
void biquad_design_peaking(biquad_t *bq, float fs, float f0, float q, float gain_db);
void biquad_design_highpass(biquad_t *bq, float fs, float f0, float q);

// Magnitude da resposta em dB na frequência f.
float biquad_mag_db(const biquad_t *bq, float fs, float f);

static inline float biquad_tick(biquad_t *bq, float x)
{
    float y = bq->b0 * x + bq->z1;
    bq->z1 = bq->b1 * x - bq->a1 * y + bq->z2;
    bq->z2 = bq->b2 * x - bq->a2 * y;
    return y;
}

void biquad_process(biquad_t *bq, float *buf, int n);
