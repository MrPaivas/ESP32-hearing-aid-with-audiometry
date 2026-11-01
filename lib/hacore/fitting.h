// Audiograma + configurações -> parâmetros do DSP.
#pragma once

#include "ha_dsp.h"
#include "ha_settings.h"
#include "ha_types.h"

// Ganho de inserção alvo por ouvido/banda (NAL-R + ajuste fino, limitado).
void fitting_targets(const ha_audiogram_t *ag, const ha_settings_t *s, float targets[HA_NUM_EARS][HA_NUM_BANDS]);

void fitting_build_params(const ha_audiogram_t *ag, const ha_settings_t *s, bool mute, ha_dsp_params_t *p);
