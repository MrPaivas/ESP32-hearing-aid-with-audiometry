#include "fitting.h"

#include "eq_fit.h"
#include "nalr.h"

void fitting_targets(const ha_audiogram_t *ag, const ha_settings_t *s, float targets[HA_NUM_EARS][HA_NUM_BANDS])
{
    for (int e = 0; e < HA_NUM_EARS; e++) {
        nalr_prescribe(ag->thr[e], s->max_test_hl, s->max_gain_db, targets[e]);
        for (int b = 0; b < HA_NUM_BANDS; b++) {
            float g = targets[e][b] + s->fine_tune_db[e][b];
            if (g < -10.0f)
                g = -10.0f;
            if (g > s->max_gain_db)
                g = s->max_gain_db;
            targets[e][b] = g;
        }
    }
}

void fitting_build_params(const ha_audiogram_t *ag, const ha_settings_t *s, bool mute, ha_dsp_params_t *p)
{
    float targets[HA_NUM_EARS][HA_NUM_BANDS];
    fitting_targets(ag, s, targets);

    // Com ganho de inserção 0 dB o som sai no mesmo SPL que chegou ao mic.
    float acoustic_ref_db = s->mic_spl_at_0dbfs - s->out_spl_at_0dbfs;

    for (int e = 0; e < HA_NUM_EARS; e++) {
        eq_fit(targets[e], HA_SAMPLE_RATE, p->eq_gain_db[e]);
        p->broadband_gain_db[e] = acoustic_ref_db + s->volume_db;
    }
    p->limiter_ceiling_dbfs = s->mpo_spl - s->out_spl_at_0dbfs;
    p->wdrc_knee_spl = s->wdrc_knee_spl;
    p->wdrc_ratio = s->wdrc_ratio;
    p->mic_spl_at_0dbfs = s->mic_spl_at_0dbfs;
    p->mute = mute;
}
