// Equalizador de 8 bandas (biquads peaking em cascata, fase mínima) e ajuste
// dos ganhos de cada biquad para que a resposta combinada atinja o alvo nas
// frequências audiométricas, compensando a sobreposição entre bandas.
#pragma once

#include "biquad.h"
#include "ha_types.h"

extern const float eq_band_q[HA_NUM_BANDS];

void eq_design_bank(biquad_t bank[HA_NUM_BANDS], float fs, const float gains_db[HA_NUM_BANDS]);

// Resposta total do banco em dB na frequência f.
float eq_response_db(const float gains_db[HA_NUM_BANDS], float fs, float f);

// Calcula os ganhos dos biquads (gains_out) que aproximam target_db.
// Retorna o erro absoluto máximo (dB) nas frequências das bandas.
float eq_fit(const float target_db[HA_NUM_BANDS], float fs, float gains_out[HA_NUM_BANDS]);
