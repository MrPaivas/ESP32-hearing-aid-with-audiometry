#include "biquad.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void biquad_set_identity(biquad_t *bq)
{
    bq->b0 = 1.0f;
    bq->b1 = bq->b2 = bq->a1 = bq->a2 = 0.0f;
}

void biquad_reset(biquad_t *bq)
{
    bq->z1 = bq->z2 = 0.0f;
}

static void set_normalized(biquad_t *bq, double b0, double b1, double b2, double a0, double a1, double a2)
{
    bq->b0 = (float)(b0 / a0);
    bq->b1 = (float)(b1 / a0);
    bq->b2 = (float)(b2 / a0);
    bq->a1 = (float)(a1 / a0);
    bq->a2 = (float)(a2 / a0);
}

void biquad_design_peaking(biquad_t *bq, float fs, float f0, float q, float gain_db)
{
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * f0 / fs;
    double alpha = sin(w0) / (2.0 * q);
    double c = cos(w0);
    set_normalized(bq, 1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
}

void biquad_design_highpass(biquad_t *bq, float fs, float f0, float q)
{
    double w0 = 2.0 * M_PI * f0 / fs;
    double alpha = sin(w0) / (2.0 * q);
    double c = cos(w0);
    set_normalized(bq, (1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
}

float biquad_mag_db(const biquad_t *bq, float fs, float f)
{
    double w = 2.0 * M_PI * f / fs;
    double c1 = cos(w), s1 = sin(w), c2 = cos(2.0 * w), s2 = sin(2.0 * w);
    double nr = bq->b0 + bq->b1 * c1 + bq->b2 * c2;
    double ni = -(bq->b1 * s1 + bq->b2 * s2);
    double dr = 1.0 + bq->a1 * c1 + bq->a2 * c2;
    double di = -(bq->a1 * s1 + bq->a2 * s2);
    double mag2 = (nr * nr + ni * ni) / (dr * dr + di * di);
    return (float)(10.0 * log10(mag2 + 1e-30));
}

void biquad_process(biquad_t *bq, float *buf, int n)
{
    for (int i = 0; i < n; i++)
        buf[i] = biquad_tick(bq, buf[i]);
}
