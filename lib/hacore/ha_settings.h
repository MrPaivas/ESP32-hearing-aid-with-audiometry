// Configurações persistentes e calibração (dB HL <-> dBFS).
#pragma once

#include "ha_types.h"

#define HA_SETTINGS_VERSION 1

typedef enum {
    TRANSDUCER_SUPRA_AURAL = 0, // fone de arco (referência TDH-39, ISO 389-1)
    TRANSDUCER_INSERT = 1,      // fone intra-auricular (referência ER-3A, ISO 389-2)
    TRANSDUCER_COUNT
} ha_transducer_t;

typedef struct {
    uint16_t version;

    // Volume do usuário (encoder) somado ao ganho prescrito.
    float volume_db;
    float volume_max_db;

    // Prescrição e segurança.
    float max_gain_db;        // teto do ganho de inserção por banda
    float mpo_spl;            // nível máximo de saída (dB SPL)
    float fine_tune_db[HA_NUM_EARS][HA_NUM_BANDS];

    // Compressão.
    float wdrc_knee_spl;
    float wdrc_ratio;

    // Calibração acústica.
    float mic_spl_at_0dbfs;   // INMP441: -26 dBFS @ 94 dB SPL -> 120
    float out_spl_at_0dbfs;   // SPL no ouvido de um seno de 1 kHz em 0 dBFS
    uint8_t transducer;
    float cal_offset_db[HA_NUM_EARS][HA_NUM_BANDS]; // calibração biológica

    // Audiometria.
    float max_test_hl;
    bool led_during_tone;     // "modo operador": LED acende junto com o tom
} ha_settings_t;

void ha_settings_defaults(ha_settings_t *s);
// Corrige valores fora de faixa (ex.: vindos da página web).
void ha_settings_sanitize(ha_settings_t *s);

float cal_retspl(uint8_t transducer, int band);
// Nível digital (dBFS) de um tom de hl dB HL no ouvido/banda.
float cal_hl_to_dbfs(const ha_settings_t *s, int ear, int band, float hl);
// Maior nível testável (múltiplo de 5), limitado por max_test_hl e por 0 dBFS.
int cal_max_hl(const ha_settings_t *s, int ear, int band);
