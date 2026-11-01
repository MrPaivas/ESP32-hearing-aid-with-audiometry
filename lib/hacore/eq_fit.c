#include "eq_fit.h"

#include <math.h>
#include <string.h>

// Q por banda, mais largo que o espaçamento das bandas: reduz os vales entre
// frequências audiométricas (pior caso ~5 dB contra ~11 dB com Q de 1 oitava);
// o eq_fit compensa a sobreposição maior.
const float eq_band_q[HA_NUM_BANDS] = {0.7f, 0.8f, 0.8f, 1.0f, 1.2f, 1.2f, 1.1f, 1.0f};

#define FIT_PROBE_DB   12.0f
#define FIT_ITERATIONS 12
#define FIT_GAIN_MIN   (-20.0f)
#define FIT_GAIN_MAX   (60.0f)

void eq_design_bank(biquad_t bank[HA_NUM_BANDS], float fs, const float gains_db[HA_NUM_BANDS])
{
    for (int b = 0; b < HA_NUM_BANDS; b++)
        biquad_design_peaking(&bank[b], fs, ha_band_freqs[b], eq_band_q[b], gains_db[b]);
}

float eq_response_db(const float gains_db[HA_NUM_BANDS], float fs, float f)
{
    biquad_t bank[HA_NUM_BANDS];
    eq_design_bank(bank, fs, gains_db);
    float db = 0.0f;
    for (int b = 0; b < HA_NUM_BANDS; b++)
        db += biquad_mag_db(&bank[b], fs, f);
    return db;
}

// Resolve A x = y (8x8) por eliminação gaussiana com pivotamento parcial.
static void solve8(float A_in[HA_NUM_BANDS][HA_NUM_BANDS], const float y[HA_NUM_BANDS], float x[HA_NUM_BANDS])
{
    const int n = HA_NUM_BANDS;
    float A[HA_NUM_BANDS][HA_NUM_BANDS + 1];
    for (int i = 0; i < n; i++) {
        memcpy(A[i], A_in[i], sizeof(float) * n);
        A[i][n] = y[i];
    }
    for (int col = 0; col < n; col++) {
        int piv = col;
        for (int r = col + 1; r < n; r++)
            if (fabsf(A[r][col]) > fabsf(A[piv][col]))
                piv = r;
        if (piv != col) {
            float tmp[HA_NUM_BANDS + 1];
            memcpy(tmp, A[col], sizeof(tmp));
            memcpy(A[col], A[piv], sizeof(tmp));
            memcpy(A[piv], tmp, sizeof(tmp));
        }
        float d = A[col][col];
        if (fabsf(d) < 1e-9f)
            d = 1e-9f;
        for (int r = col + 1; r < n; r++) {
            float f = A[r][col] / d;
            for (int c = col; c <= n; c++)
                A[r][c] -= f * A[col][c];
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        float s = A[i][n];
        for (int c = i + 1; c < n; c++)
            s -= A[i][c] * x[c];
        float d = fabsf(A[i][i]) < 1e-9f ? 1e-9f : A[i][i];
        x[i] = s / d;
    }
}

float eq_fit(const float target_db[HA_NUM_BANDS], float fs, float gains_out[HA_NUM_BANDS])
{
    // Jacobiano aproximado: contribuição (dB por dB) de cada banda j em cada
    // frequência i. Usado como passo de Newton com Jacobiano fixo.
    float J[HA_NUM_BANDS][HA_NUM_BANDS];
    for (int j = 0; j < HA_NUM_BANDS; j++) {
        biquad_t bq;
        biquad_design_peaking(&bq, fs, ha_band_freqs[j], eq_band_q[j], FIT_PROBE_DB);
        for (int i = 0; i < HA_NUM_BANDS; i++)
            J[i][j] = biquad_mag_db(&bq, fs, ha_band_freqs[i]) / FIT_PROBE_DB;
    }

    float g[HA_NUM_BANDS];
    solve8(J, target_db, g);

    float max_err = 0.0f;
    for (int it = 0; it <= FIT_ITERATIONS; it++) {
        for (int b = 0; b < HA_NUM_BANDS; b++) {
            if (g[b] < FIT_GAIN_MIN)
                g[b] = FIT_GAIN_MIN;
            if (g[b] > FIT_GAIN_MAX)
                g[b] = FIT_GAIN_MAX;
        }
        float resid[HA_NUM_BANDS];
        max_err = 0.0f;
        for (int i = 0; i < HA_NUM_BANDS; i++) {
            resid[i] = target_db[i] - eq_response_db(g, fs, ha_band_freqs[i]);
            if (fabsf(resid[i]) > max_err)
                max_err = fabsf(resid[i]);
        }
        if (max_err < 0.05f || it == FIT_ITERATIONS)
            break;
        float delta[HA_NUM_BANDS];
        solve8(J, resid, delta);
        for (int b = 0; b < HA_NUM_BANDS; b++)
            g[b] += delta[b];
    }

    memcpy(gains_out, g, sizeof(g));
    return max_err;
}
