#include "audiometry.h"

#include <string.h>

#define RETEST_STEP 6

static const int seq_band[AUD_STEPS_PER_EAR] = {BAND_1K, BAND_2K, BAND_3K, BAND_4K, BAND_6K,
                                                BAND_8K, BAND_1K, BAND_500, BAND_250};

// Banda cujo limiar orienta o nível inicial de cada passo.
static const int ref_band[AUD_STEPS_PER_EAR] = {-1, BAND_1K, BAND_2K, BAND_3K, BAND_4K,
                                                BAND_6K, BAND_1K, BAND_1K, BAND_500};

#define FIRST_START_HL 40 // primeira frequência de cada ouvido (familiarização)
#define START_HL       30

static int level_index(int level)
{
    return (level - HA_HL_MIN) / HA_HL_STEP;
}

static int cur_ear(const aud_state_t *s)
{
    return s->ear_order[s->step / AUD_STEPS_PER_EAR];
}

static int cur_pos(const aud_state_t *s)
{
    return s->step % AUD_STEPS_PER_EAR;
}

static int clamp_level(const aud_state_t *s, int level)
{
    int max = s->max_level[cur_ear(s)][seq_band[cur_pos(s)]];
    if (level > max)
        level = max;
    if (level < HA_HL_MIN)
        level = HA_HL_MIN;
    return level;
}

static void start_step(aud_state_t *s)
{
    if (aud_done(s))
        return;
    int ear = cur_ear(s);
    int pos = cur_pos(s);

    int start = FIRST_START_HL;
    if (pos > 0) {
        start = START_HL;
        int8_t ref = s->thr[ear][ref_band[pos]];
        if (ha_thr_is_measured(ref) && ref + 10 > start)
            start = ref + 10;
    }
    s->level = clamp_level(s, start);
    s->searching = false;
    s->last_heard = false;
    s->trials = 0;
    s->no_resp_at_max = 0;
    memset(s->asc_count, 0, sizeof(s->asc_count));
    memset(s->asc_resp, 0, sizeof(s->asc_resp));
}

static void finish_step(aud_state_t *s, int8_t value)
{
    int ear = cur_ear(s);
    int pos = cur_pos(s);
    if (pos == RETEST_STEP)
        s->retest_1k[ear] = value;
    else
        s->thr[ear][seq_band[pos]] = value;
    s->step++;
    start_step(s);
}

void aud_init(aud_state_t *s, const int *ears, int n_ears, const int max_level[HA_NUM_EARS][HA_NUM_BANDS])
{
    memset(s, 0, sizeof(*s));
    if (n_ears > HA_NUM_EARS)
        n_ears = HA_NUM_EARS;
    s->n_ears = n_ears;
    for (int i = 0; i < n_ears; i++)
        s->ear_order[i] = ears[i];
    for (int e = 0; e < HA_NUM_EARS; e++) {
        s->retest_1k[e] = HA_THR_NOT_TESTED;
        for (int b = 0; b < HA_NUM_BANDS; b++) {
            s->thr[e][b] = HA_THR_NOT_TESTED;
            // Níveis sempre em múltiplos de 5 dB.
            int m = max_level[e][b];
            m -= ((m - HA_HL_MIN) % HA_HL_STEP + HA_HL_STEP) % HA_HL_STEP;
            s->max_level[e][b] = m < HA_HL_MIN ? HA_HL_MIN : (m > HA_HL_MAX ? HA_HL_MAX : m);
        }
    }
    start_step(s);
}

bool aud_done(const aud_state_t *s)
{
    return s->step >= aud_total_steps(s);
}

int aud_total_steps(const aud_state_t *s)
{
    return s->n_ears * AUD_STEPS_PER_EAR;
}

bool aud_current_trial(const aud_state_t *s, aud_trial_t *t)
{
    if (aud_done(s))
        return false;
    t->ear = cur_ear(s);
    t->band = seq_band[cur_pos(s)];
    t->level_hl = s->level;
    t->retest = cur_pos(s) == RETEST_STEP;
    return true;
}

// Menor nível com ao menos uma resposta ascendente (usado quando o
// procedimento não converge).
static int8_t fallback_threshold(const aud_state_t *s)
{
    for (int i = 0; i < AUD_NUM_LEVELS; i++)
        if (s->asc_resp[i] > 0)
            return (int8_t)(HA_HL_MIN + i * HA_HL_STEP);
    return HA_THR_NO_RESPONSE;
}

void aud_submit(aud_state_t *s, bool heard)
{
    if (aud_done(s))
        return;

    int lvl = s->level;
    int idx = level_index(lvl);
    int max = s->max_level[cur_ear(s)][seq_band[cur_pos(s)]];
    s->trials++;

    if (!s->searching) {
        // Fase inicial/familiarização: sobe 10 dB até a primeira resposta.
        if (heard) {
            s->searching = true;
            s->last_heard = true;
            s->level = clamp_level(s, lvl - 10);
        } else if (lvl >= max) {
            if (++s->no_resp_at_max >= 2)
                finish_step(s, HA_THR_NO_RESPONSE);
        } else {
            s->level = clamp_level(s, lvl + 10);
        }
        return;
    }

    // Uma apresentação é ascendente quando vem depois de uma não-resposta.
    // No nível mínimo não dá para descer, então também conta.
    bool ascending = !s->last_heard || lvl == HA_HL_MIN;
    if (ascending) {
        s->asc_count[idx]++;
        if (heard && ++s->asc_resp[idx] >= 2) {
            finish_step(s, (int8_t)lvl);
            return;
        }
    }
    s->last_heard = heard;

    if (heard) {
        s->level = clamp_level(s, lvl - 10);
    } else if (lvl >= max) {
        if (++s->no_resp_at_max >= 2) {
            finish_step(s, s->asc_resp[idx] > 0 ? (int8_t)lvl : HA_THR_NO_RESPONSE);
            return;
        }
    } else {
        s->level = clamp_level(s, lvl + 5);
    }

    if (s->trials >= AUD_MAX_TRIALS_PER_STEP)
        finish_step(s, fallback_threshold(s));
}

void aud_get_audiogram(const aud_state_t *s, ha_audiogram_t *ag)
{
    memcpy(ag->thr, s->thr, sizeof(ag->thr));
    for (int e = 0; e < HA_NUM_EARS; e++) {
        int8_t a = s->thr[e][BAND_1K];
        int8_t b = s->retest_1k[e];
        if (ha_thr_is_measured(a) && ha_thr_is_measured(b))
            ag->thr[e][BAND_1K] = a < b ? a : b;
        else if (!ha_thr_is_measured(a) && ha_thr_is_measured(b))
            ag->thr[e][BAND_1K] = b;
    }
}

int aud_retest_diff(const aud_state_t *s, int ear)
{
    int8_t a = s->thr[ear][BAND_1K];
    int8_t b = s->retest_1k[ear];
    if (!ha_thr_is_measured(a) || !ha_thr_is_measured(b))
        return -1;
    return a > b ? a - b : b - a;
}
