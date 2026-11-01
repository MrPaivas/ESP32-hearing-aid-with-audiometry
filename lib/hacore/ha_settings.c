#include "ha_settings.h"

#include <math.h>
#include <string.h>

// RETSPL (dB SPL equivalente a 0 dB HL) por banda.
static const float retspl[TRANSDUCER_COUNT][HA_NUM_BANDS] = {
    // 250   500   1k   2k   3k    4k   6k    8k
    {25.5f, 11.5f, 7.0f, 9.0f, 10.0f, 9.5f, 15.5f, 13.0f}, // TDH-39 (ISO 389-1)
    {14.0f, 5.5f, 0.0f, 3.0f, 3.5f, 5.5f, 2.0f, 0.0f},      // ER-3A (ISO 389-2)
};

static float clampf(float v, float lo, float hi)
{
    if (!(v >= lo)) // também trata NaN
        return lo;
    return v > hi ? hi : v;
}

void ha_settings_defaults(ha_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->version = HA_SETTINGS_VERSION;
    s->volume_db = 0.0f;
    s->volume_max_db = 10.0f;
    s->max_gain_db = 40.0f;
    s->mpo_spl = 100.0f;
    s->wdrc_knee_spl = 55.0f;
    s->wdrc_ratio = 2.0f;
    s->mic_spl_at_0dbfs = 120.0f;
    s->out_spl_at_0dbfs = 110.0f;
    s->transducer = TRANSDUCER_SUPRA_AURAL;
    s->max_test_hl = 90.0f;
    s->led_during_tone = false;
}

void ha_settings_sanitize(ha_settings_t *s)
{
    s->version = HA_SETTINGS_VERSION;
    s->volume_max_db = clampf(s->volume_max_db, 0.0f, 20.0f);
    s->volume_db = clampf(s->volume_db, -40.0f, s->volume_max_db);
    s->max_gain_db = clampf(s->max_gain_db, 0.0f, 50.0f);
    s->mpo_spl = clampf(s->mpo_spl, 70.0f, 110.0f);
    s->wdrc_knee_spl = clampf(s->wdrc_knee_spl, 30.0f, 90.0f);
    s->wdrc_ratio = clampf(s->wdrc_ratio, 1.0f, 4.0f);
    s->mic_spl_at_0dbfs = clampf(s->mic_spl_at_0dbfs, 100.0f, 140.0f);
    s->out_spl_at_0dbfs = clampf(s->out_spl_at_0dbfs, 80.0f, 130.0f);
    if (s->transducer >= TRANSDUCER_COUNT)
        s->transducer = TRANSDUCER_SUPRA_AURAL;
    s->max_test_hl = clampf(s->max_test_hl, 20.0f, 100.0f);
    for (int e = 0; e < HA_NUM_EARS; e++) {
        for (int b = 0; b < HA_NUM_BANDS; b++) {
            s->fine_tune_db[e][b] = clampf(s->fine_tune_db[e][b], -20.0f, 20.0f);
            s->cal_offset_db[e][b] = clampf(s->cal_offset_db[e][b], -40.0f, 40.0f);
        }
    }
}

float cal_retspl(uint8_t transducer, int band)
{
    if (transducer >= TRANSDUCER_COUNT)
        transducer = TRANSDUCER_SUPRA_AURAL;
    return retspl[transducer][band];
}

float cal_hl_to_dbfs(const ha_settings_t *s, int ear, int band, float hl)
{
    float spl = hl + cal_retspl(s->transducer, band) + s->cal_offset_db[ear][band];
    return spl - s->out_spl_at_0dbfs;
}

int cal_max_hl(const ha_settings_t *s, int ear, int band)
{
    // Nível em que o tom chega a -1 dBFS.
    float hw_max = s->out_spl_at_0dbfs - 1.0f - cal_retspl(s->transducer, band) - s->cal_offset_db[ear][band];
    float m = hw_max < s->max_test_hl ? hw_max : s->max_test_hl;
    int lvl = (int)floorf(m / HA_HL_STEP) * HA_HL_STEP;
    if (lvl > HA_HL_MAX)
        lvl = HA_HL_MAX;
    if (lvl < HA_HL_MIN)
        lvl = HA_HL_MIN;
    return lvl;
}
