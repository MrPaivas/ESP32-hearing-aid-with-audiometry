// Prescrição de ganho NAL-R (Byrne & Dillon, 1986).
#pragma once

#include "ha_types.h"

// Limiar usado numa banda: interpola bandas não testadas (em log-frequência)
// e substitui "sem resposta" por no_resp_hl. Retorna false se o ouvido não
// tem nenhuma medida.
bool nalr_effective_thresholds(const int8_t thr[HA_NUM_BANDS], float no_resp_hl, float out_hl[HA_NUM_BANDS]);

// Ganho de inserção NAL-R por banda, limitado a [0, max_gain_db].
// Sem dados, todos os ganhos ficam em 0 dB.
void nalr_prescribe(const int8_t thr[HA_NUM_BANDS], float no_resp_hl, float max_gain_db, float gain_out[HA_NUM_BANDS]);
