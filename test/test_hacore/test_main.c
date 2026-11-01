// Testes da lógica pura: pio test -e native
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "audiometry.h"
#include "biquad.h"
#include "dynamics.h"
#include "eq_fit.h"
#include "fitting.h"
#include "ha_settings.h"
#include "nalr.h"
#include "tone_gen.h"

void setUp(void) {}
void tearDown(void) {}

static void fill(int8_t thr[HA_NUM_BANDS], int v)
{
    for (int b = 0; b < HA_NUM_BANDS; b++)
        thr[b] = (int8_t)v;
}

// ---------- NAL-R ----------

static void test_nalr_flat_40(void)
{
    int8_t thr[HA_NUM_BANDS];
    fill(thr, 40);
    float g[HA_NUM_BANDS];
    nalr_prescribe(thr, 90, 60, g);
    // X = 0,05 * 120 = 6; IG = 6 + 12,4 + k
    const float expected[HA_NUM_BANDS] = {1.4f, 10.4f, 19.4f, 17.4f, 16.4f, 16.4f, 16.4f, 16.4f};
    for (int b = 0; b < HA_NUM_BANDS; b++)
        TEST_ASSERT_FLOAT_WITHIN(0.01f, expected[b], g[b]);
}

static void test_nalr_normal_hearing_gives_no_gain(void)
{
    int8_t thr[HA_NUM_BANDS];
    fill(thr, 0);
    float g[HA_NUM_BANDS];
    nalr_prescribe(thr, 90, 60, g);
    // Só 1 kHz tem k positivo (+1 dB).
    for (int b = 0; b < HA_NUM_BANDS; b++)
        TEST_ASSERT_FLOAT_WITHIN(0.01f, b == BAND_1K ? 1.0f : 0.0f, g[b]);
}

static void test_nalr_clamps_to_max_gain(void)
{
    int8_t thr[HA_NUM_BANDS];
    fill(thr, 100);
    float g[HA_NUM_BANDS];
    nalr_prescribe(thr, 90, 30, g);
    for (int b = 1; b < HA_NUM_BANDS; b++)
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, g[b]);
}

static void test_nalr_interpolates_missing_bands(void)
{
    int8_t thr[HA_NUM_BANDS];
    fill(thr, HA_THR_NOT_TESTED);
    thr[BAND_2K] = 20;
    thr[BAND_4K] = 60;
    float h[HA_NUM_BANDS];
    TEST_ASSERT_TRUE(nalr_effective_thresholds(thr, 90, h));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, h[BAND_250]); // extrapola pela borda
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, h[BAND_8K]);
    // 3 kHz fica a log2(1,5) = 0,585 do caminho entre 2k e 4k.
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f + 0.585f * 40.0f, h[BAND_3K]);
}

static void test_nalr_no_data(void)
{
    int8_t thr[HA_NUM_BANDS];
    fill(thr, HA_THR_NOT_TESTED);
    float g[HA_NUM_BANDS];
    nalr_prescribe(thr, 90, 40, g);
    for (int b = 0; b < HA_NUM_BANDS; b++)
        TEST_ASSERT_EQUAL_FLOAT(0.0f, g[b]);
}

// ---------- Biquad / EQ ----------

static void test_peaking_gain_at_center(void)
{
    biquad_t bq;
    biquad_design_peaking(&bq, HA_SAMPLE_RATE, 1000, 1.4f, 12.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, biquad_mag_db(&bq, HA_SAMPLE_RATE, 1000));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, biquad_mag_db(&bq, HA_SAMPLE_RATE, 10));
}

static void test_biquad_time_domain_matches_magnitude(void)
{
    biquad_t bq;
    biquad_design_peaking(&bq, HA_SAMPLE_RATE, 2000, 1.8f, 10.0f);
    biquad_reset(&bq);
    double in_e = 0, out_e = 0;
    for (int i = 0; i < HA_SAMPLE_RATE; i++) {
        float x = sinf(2.0f * 3.14159265f * 2000.0f * i / HA_SAMPLE_RATE);
        float y = biquad_tick(&bq, x);
        if (i > HA_SAMPLE_RATE / 2) {
            in_e += x * x;
            out_e += y * y;
        }
    }
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f, (float)(10.0 * log10(out_e / in_e)));
}

static void check_fit(const float target[HA_NUM_BANDS])
{
    float g[HA_NUM_BANDS];
    float err = eq_fit(target, HA_SAMPLE_RATE, g);
    TEST_ASSERT_TRUE_MESSAGE(err < 2.0f, "erro do ajuste >= 2 dB");
    for (int b = 0; b < HA_NUM_BANDS; b++)
        TEST_ASSERT_FLOAT_WITHIN(2.0f, target[b], eq_response_db(g, HA_SAMPLE_RATE, ha_band_freqs[b]));
}

static void test_eq_fit_flat_loss(void)
{
    const float t[HA_NUM_BANDS] = {1.4f, 10.4f, 19.4f, 17.4f, 16.4f, 16.4f, 16.4f, 16.4f};
    check_fit(t);
}

static void test_eq_fit_sloping_loss(void)
{
    // NAL-R de uma perda descendente 20 -> 75 dB HL
    int8_t thr[HA_NUM_BANDS] = {20, 25, 30, 45, 55, 65, 70, 75};
    float t[HA_NUM_BANDS];
    nalr_prescribe(thr, 90, 40, t);
    check_fit(t);
}

static void test_eq_fit_zero(void)
{
    const float t[HA_NUM_BANDS] = {0};
    check_fit(t);
}

// ---------- Dinâmica ----------

static void test_limiter_never_exceeds_ceiling(void)
{
    limiter_t l;
    limiter_init(&l, HA_SAMPLE_RATE, 50);
    limiter_set_ceiling_db(&l, -10.0f);
    float buf[256];
    for (int i = 0; i < 256; i++)
        buf[i] = 0.9f * sinf(i * 0.3f);
    limiter_process(&l, buf, 256);
    float ceil = powf(10.0f, -0.5f);
    for (int i = 0; i < 256; i++)
        TEST_ASSERT_TRUE(fabsf(buf[i]) <= ceil + 1e-6f);
}

static void test_wdrc_ratio(void)
{
    wdrc_t c;
    wdrc_init(&c, 1000.0f, 0.0f, 0.0f); // sem suavização
    wdrc_set(&c, 50.0f, 2.0f, 120.0f);
    // Seno com RMS em 80 dB SPL: 30 dB acima do joelho -> -15 dB.
    float amp = powf(10.0f, (80.0f - 120.0f) / 20.0f) * sqrtf(2.0f);
    float buf[320];
    for (int i = 0; i < 320; i++)
        buf[i] = amp * sinf(2.0f * 3.14159265f * i / 32.0f);
    float g = wdrc_block(&c, buf, 320);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, -15.0f, 20.0f * log10f(g));
    // Abaixo do joelho, ganho unitário.
    for (int i = 0; i < 320; i++)
        buf[i] *= 0.001f;
    g = wdrc_block(&c, buf, 320);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, 20.0f * log10f(g));
}

// ---------- Tom ----------

static void test_tone_ramps_and_length(void)
{
    tone_gen_t t;
    tone_gen_start(&t, 32000, 1000, 0.5f, 2, 100, 50, 20);
    static float buf[32000];
    int n = 0;
    while (tone_gen_active(&t) && n < 32000) {
        tone_gen_render(&t, &buf[n], 32);
        n += 32;
    }
    // 2 x 100 ms + 50 ms = 250 ms = 8000 amostras (arredondado ao bloco)
    TEST_ASSERT_INT_WITHIN(32, 8000, n);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, buf[0]);
    float peak = 0;
    for (int i = 0; i < n; i++)
        peak = fmaxf(peak, fabsf(buf[i]));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, peak);
    // Silêncio entre os pulsos.
    for (int i = 3200 + 10; i < 4800 - 10; i++)
        TEST_ASSERT_EQUAL_FLOAT(0.0f, buf[i]);
}

static void test_tone_stop_is_smooth(void)
{
    tone_gen_t t;
    tone_gen_start(&t, 32000, 1000, 1.0f, 1, 500, 0, 20);
    float buf[64];
    for (int i = 0; i < 50; i++)
        tone_gen_render(&t, buf, 64);
    tone_gen_stop(&t);
    int n = 0;
    float blk[32];
    float first = -1.0f;
    while (tone_gen_active(&t) && n < 2000) {
        tone_gen_render(&t, blk, 32);
        if (first < 0.0f)
            first = fabsf(blk[8]); // pico do primeiro ciclo após o stop
        n += 32;
    }
    TEST_ASSERT_INT_WITHIN(32, 640, n);        // rampa de 20 ms
    TEST_ASSERT_TRUE(first > 0.9f);            // sem salto para zero (clique)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, blk[31]); // termina em silêncio
}

// ---------- Audiometria ----------

typedef struct {
    int thr[HA_NUM_EARS][HA_NUM_BANDS]; // limiar verdadeiro (dB HL); 999 = surdo
    int flip_every; // 0 = determinístico; N = inverte cada N-ésima resposta
} listener_t;

static void run_listener(const listener_t *L, int max_hl, ha_audiogram_t *out, aud_state_t *st)
{
    int max_level[HA_NUM_EARS][HA_NUM_BANDS];
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            max_level[e][b] = max_hl;
    const int ears[] = {HA_EAR_RIGHT, HA_EAR_LEFT};
    aud_init(st, ears, 2, max_level);
    aud_trial_t t;
    int count = 0, guard = 0;
    while (aud_current_trial(st, &t)) {
        bool heard = t.level_hl >= L->thr[t.ear][t.band];
        if (L->flip_every && ++count % L->flip_every == 0)
            heard = !heard;
        aud_submit(st, heard);
        TEST_ASSERT_TRUE(++guard < 5000);
    }
    aud_get_audiogram(st, out);
}

static void test_audiometry_exact_thresholds(void)
{
    listener_t L = {.thr = {{-10, -5, 0, 15, 35, 50, 70, 90}, {5, 10, 20, 25, 40, 55, 999, 999}}};
    static aud_state_t st;
    ha_audiogram_t ag;
    run_listener(&L, 90, &ag, &st);
    for (int b = 0; b < HA_NUM_BANDS; b++) {
        TEST_ASSERT_EQUAL_INT(L.thr[HA_EAR_LEFT][b], ag.thr[HA_EAR_LEFT][b]);
        int exp = L.thr[HA_EAR_RIGHT][b] > 90 ? HA_THR_NO_RESPONSE : L.thr[HA_EAR_RIGHT][b];
        TEST_ASSERT_EQUAL_INT(exp, ag.thr[HA_EAR_RIGHT][b]);
    }
    TEST_ASSERT_EQUAL_INT(0, aud_retest_diff(&st, HA_EAR_LEFT));
}

static void test_audiometry_rounds_up_between_steps(void)
{
    listener_t L;
    memset(&L, 0, sizeof(L));
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            L.thr[e][b] = 37;
    static aud_state_t st;
    ha_audiogram_t ag;
    run_listener(&L, 90, &ag, &st);
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            TEST_ASSERT_EQUAL_INT(40, ag.thr[e][b]);
}

static void test_audiometry_tolerates_occasional_errors(void)
{
    listener_t L = {.flip_every = 9};
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            L.thr[e][b] = 30 + 5 * b;
    static aud_state_t st;
    ha_audiogram_t ag;
    run_listener(&L, 90, &ag, &st);
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            TEST_ASSERT_INT_WITHIN(10, L.thr[e][b], ag.thr[e][b]);
}

static void test_audiometry_order_right_first(void)
{
    int max_level[HA_NUM_EARS][HA_NUM_BANDS];
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            max_level[e][b] = 90;
    const int ears[] = {HA_EAR_RIGHT, HA_EAR_LEFT};
    static aud_state_t st;
    aud_init(&st, ears, 2, max_level);
    aud_trial_t t;
    TEST_ASSERT_TRUE(aud_current_trial(&st, &t));
    TEST_ASSERT_EQUAL_INT(HA_EAR_RIGHT, t.ear);
    TEST_ASSERT_EQUAL_INT(BAND_1K, t.band);
    TEST_ASSERT_EQUAL_INT(40, t.level_hl);
}

// ---------- Calibração / fitting ----------

static void test_calibration_levels(void)
{
    ha_settings_t s;
    ha_settings_defaults(&s);
    // 0 dB HL a 1 kHz, TDH-39: 7 dB SPL -> 7 - 110 = -103 dBFS
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -103.0f, cal_hl_to_dbfs(&s, 0, BAND_1K, 0));
    s.cal_offset_db[0][BAND_1K] = 5;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -98.0f, cal_hl_to_dbfs(&s, 0, BAND_1K, 0));
    // Nível máximo limitado por max_test_hl (90) e múltiplo de 5.
    TEST_ASSERT_EQUAL_INT(90, cal_max_hl(&s, 0, BAND_1K));
    s.out_spl_at_0dbfs = 90;
    // 90 - 1 - 25,5 (250 Hz) = 63,5 -> 60
    TEST_ASSERT_EQUAL_INT(60, cal_max_hl(&s, 0, BAND_250));
}

static void test_fitting_params(void)
{
    ha_settings_t s;
    ha_settings_defaults(&s);
    ha_audiogram_t ag;
    ha_audiogram_clear(&ag);
    ha_dsp_params_t p;
    fitting_build_params(&ag, &s, false, &p);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, p.broadband_gain_db[0]); // 120 - 110 + volume 0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, p.limiter_ceiling_dbfs); // 100 - 110
}

static void test_dsp_output_bounded(void)
{
    static ha_dsp_t d;
    ha_dsp_init(&d, HA_SAMPLE_RATE);
    ha_settings_t s;
    ha_settings_defaults(&s);
    ha_audiogram_t ag;
    for (int e = 0; e < HA_NUM_EARS; e++)
        fill(ag.thr[e], 70);
    ha_dsp_params_t p;
    fitting_build_params(&ag, &s, false, &p);
    ha_dsp_configure(&d, &p);
    float ceil = powf(10.0f, p.limiter_ceiling_dbfs / 20.0f);
    float in[HA_BLOCK_SIZE], l[HA_BLOCK_SIZE], r[HA_BLOCK_SIZE];
    srand(1);
    for (int blk = 0; blk < 2000; blk++) {
        for (int i = 0; i < HA_BLOCK_SIZE; i++)
            in[i] = ((float)rand() / RAND_MAX - 0.5f) * 1.8f; // ruído alto
        ha_dsp_process(&d, in, l, r, HA_BLOCK_SIZE);
        for (int i = 0; i < HA_BLOCK_SIZE; i++) {
            TEST_ASSERT_TRUE(fabsf(l[i]) <= ceil + 1e-5f);
            TEST_ASSERT_TRUE(fabsf(r[i]) <= ceil + 1e-5f);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nalr_flat_40);
    RUN_TEST(test_nalr_normal_hearing_gives_no_gain);
    RUN_TEST(test_nalr_clamps_to_max_gain);
    RUN_TEST(test_nalr_interpolates_missing_bands);
    RUN_TEST(test_nalr_no_data);
    RUN_TEST(test_peaking_gain_at_center);
    RUN_TEST(test_biquad_time_domain_matches_magnitude);
    RUN_TEST(test_eq_fit_flat_loss);
    RUN_TEST(test_eq_fit_sloping_loss);
    RUN_TEST(test_eq_fit_zero);
    RUN_TEST(test_limiter_never_exceeds_ceiling);
    RUN_TEST(test_wdrc_ratio);
    RUN_TEST(test_tone_ramps_and_length);
    RUN_TEST(test_tone_stop_is_smooth);
    RUN_TEST(test_audiometry_exact_thresholds);
    RUN_TEST(test_audiometry_rounds_up_between_steps);
    RUN_TEST(test_audiometry_tolerates_occasional_errors);
    RUN_TEST(test_audiometry_order_right_first);
    RUN_TEST(test_calibration_levels);
    RUN_TEST(test_fitting_params);
    RUN_TEST(test_dsp_output_bounded);
    return UNITY_END();
}
