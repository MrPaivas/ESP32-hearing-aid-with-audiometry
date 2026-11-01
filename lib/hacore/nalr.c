#include "nalr.h"

#include <math.h>

// Correções k(f) da tabela NAL-R. 8 kHz não consta no original; usa-se o
// mesmo valor de 6 kHz.
static const float k_nalr[HA_NUM_BANDS] = {-17, -8, 1, -1, -2, -2, -2, -2};

bool nalr_effective_thresholds(const int8_t thr[HA_NUM_BANDS], float no_resp_hl, float out_hl[HA_NUM_BANDS])
{
    float val[HA_NUM_BANDS];
    bool has[HA_NUM_BANDS];
    bool any = false;

    for (int b = 0; b < HA_NUM_BANDS; b++) {
        has[b] = thr[b] != HA_THR_NOT_TESTED;
        val[b] = thr[b] == HA_THR_NO_RESPONSE ? no_resp_hl : (float)thr[b];
        any |= has[b];
    }
    if (!any) {
        for (int b = 0; b < HA_NUM_BANDS; b++)
            out_hl[b] = 0.0f;
        return false;
    }

    for (int b = 0; b < HA_NUM_BANDS; b++) {
        if (has[b]) {
            out_hl[b] = val[b];
            continue;
        }
        int lo = b - 1, hi = b + 1;
        while (lo >= 0 && !has[lo])
            lo--;
        while (hi < HA_NUM_BANDS && !has[hi])
            hi++;
        if (lo < 0) {
            out_hl[b] = val[hi];
        } else if (hi >= HA_NUM_BANDS) {
            out_hl[b] = val[lo];
        } else {
            float x = log2f(ha_band_freqs[b] / ha_band_freqs[lo]) / log2f(ha_band_freqs[hi] / ha_band_freqs[lo]);
            out_hl[b] = val[lo] + x * (val[hi] - val[lo]);
        }
    }
    return true;
}

void nalr_prescribe(const int8_t thr[HA_NUM_BANDS], float no_resp_hl, float max_gain_db, float gain_out[HA_NUM_BANDS])
{
    float h[HA_NUM_BANDS];
    if (!nalr_effective_thresholds(thr, no_resp_hl, h)) {
        for (int b = 0; b < HA_NUM_BANDS; b++)
            gain_out[b] = 0.0f;
        return;
    }

    // X = 0,15 * média(H500, H1k, H2k)
    float x = 0.05f * (h[BAND_500] + h[BAND_1K] + h[BAND_2K]);
    for (int b = 0; b < HA_NUM_BANDS; b++) {
        float g = x + 0.31f * h[b] + k_nalr[b];
        if (g < 0.0f)
            g = 0.0f;
        if (g > max_gain_db)
            g = max_gain_db;
        gain_out[b] = g;
    }
}
