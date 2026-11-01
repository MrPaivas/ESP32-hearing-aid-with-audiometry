#include "ha_types.h"

const float ha_band_freqs[HA_NUM_BANDS] = {250, 500, 1000, 2000, 3000, 4000, 6000, 8000};

void ha_audiogram_clear(ha_audiogram_t *ag)
{
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            ag->thr[e][b] = HA_THR_NOT_TESTED;
}

bool ha_audiogram_has_data(const ha_audiogram_t *ag, int ear)
{
    for (int b = 0; b < HA_NUM_BANDS; b++)
        if (ag->thr[ear][b] != HA_THR_NOT_TESTED)
            return true;
    return false;
}
