// Procedimento de audiometria tonal Hughson-Westlake modificado
// (ASHA 2005 / ISO 8253-1): desce 10 dB após resposta, sobe 5 dB sem resposta;
// limiar = menor nível com 2 respostas em apresentações ascendentes.
//
// Lógica pura, sem temporização: o chamador apresenta o tom de
// aud_current_trial() e informa o resultado com aud_submit().
#pragma once

#include "ha_types.h"

#define AUD_STEPS_PER_EAR 9 // 1k 2k 3k 4k 6k 8k 1k(reteste) 500 250
#define AUD_NUM_LEVELS    ((HA_HL_MAX - HA_HL_MIN) / HA_HL_STEP + 1)
#define AUD_MAX_TRIALS_PER_STEP 40

typedef struct {
    int ear;
    int band;
    int level_hl;
    bool retest;
} aud_trial_t;

typedef struct {
    // Configuração
    int ear_order[HA_NUM_EARS];
    int n_ears;
    int max_level[HA_NUM_EARS][HA_NUM_BANDS];

    // Progresso
    int step;          // 0 .. n_ears * AUD_STEPS_PER_EAR
    int level;
    bool searching;    // já houve a primeira resposta nesta frequência
    bool last_heard;
    int trials;
    int no_resp_at_max;
    uint8_t asc_count[AUD_NUM_LEVELS];
    uint8_t asc_resp[AUD_NUM_LEVELS];

    // Resultados
    int8_t thr[HA_NUM_EARS][HA_NUM_BANDS];
    int8_t retest_1k[HA_NUM_EARS];
} aud_state_t;

// ears: lista de ouvidos na ordem de teste (normalmente direito, depois esquerdo).
void aud_init(aud_state_t *s, const int *ears, int n_ears, const int max_level[HA_NUM_EARS][HA_NUM_BANDS]);
bool aud_done(const aud_state_t *s);
bool aud_current_trial(const aud_state_t *s, aud_trial_t *t);
void aud_submit(aud_state_t *s, bool heard);
int aud_total_steps(const aud_state_t *s);

// Resultado final; no 1 kHz usa o menor entre teste e reteste.
void aud_get_audiogram(const aud_state_t *s, ha_audiogram_t *ag);
// Diferença |teste - reteste| em 1 kHz (-1 se indisponível). >5 dB sugere
// baixa confiabilidade.
int aud_retest_diff(const aud_state_t *s, int ear);
