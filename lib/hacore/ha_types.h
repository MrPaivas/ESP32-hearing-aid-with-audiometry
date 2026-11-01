// Tipos e constantes compartilhados entre firmware e testes nativos.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HA_SAMPLE_RATE 32000
#define HA_BLOCK_SIZE  32 // 1 ms a 32 kHz

#define HA_NUM_BANDS 8
#define HA_NUM_EARS  2
#define HA_EAR_LEFT  0
#define HA_EAR_RIGHT 1

// Faixa de níveis da audiometria (dB HL), passo de 5 dB.
#define HA_HL_MIN  (-10)
#define HA_HL_MAX  (120)
#define HA_HL_STEP 5

// Valores especiais no audiograma.
#define HA_THR_NOT_TESTED  (-128)
#define HA_THR_NO_RESPONSE (127)

// Índices das bandas em ha_band_freqs.
enum { BAND_250, BAND_500, BAND_1K, BAND_2K, BAND_3K, BAND_4K, BAND_6K, BAND_8K };

extern const float ha_band_freqs[HA_NUM_BANDS];

typedef struct {
    int8_t thr[HA_NUM_EARS][HA_NUM_BANDS]; // limiar em dB HL
} ha_audiogram_t;

static inline bool ha_thr_is_measured(int8_t t)
{
    return t >= HA_HL_MIN && t <= HA_HL_MAX;
}

void ha_audiogram_clear(ha_audiogram_t *ag);
bool ha_audiogram_has_data(const ha_audiogram_t *ag, int ear);
