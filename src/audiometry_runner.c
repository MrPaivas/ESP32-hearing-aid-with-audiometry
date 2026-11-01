#include "audiometry_runner.h"

#include <string.h>

#include "app.h"
#include "audio_engine.h"
#include "audiometry.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "leds.h"

static const char *TAG = "audiometry";

// Tom pulsado: 3 x (250 ms ligado / 150 ms desligado) ≈ 1,05 s.
#define TONE_PULSES     3
#define TONE_ON_MS      250
#define TONE_OFF_MS     150
#define TONE_TOTAL_MS   (TONE_PULSES * TONE_ON_MS + (TONE_PULSES - 1) * TONE_OFF_MS)
#define RESPONSE_EXTRA_MS 1000 // janela de resposta após o fim do tom
#define GAP_MIN_MS      1500   // intervalo aleatório entre apresentações
#define GAP_RAND_MS     2500
#define GAP_AFTER_FP_MS 2000   // reinício do intervalo após falso positivo

static TaskHandle_t s_task;
static volatile bool s_abort;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static audiometry_status_t s_status = {.retest_diff = {-1, -1}};

static void set_status_trial(const aud_state_t *st, const aud_trial_t *t, int fp)
{
    taskENTER_CRITICAL(&s_lock);
    s_status.step = st->step;
    s_status.total_steps = aud_total_steps(st);
    s_status.ear = t->ear;
    s_status.band = t->band;
    s_status.level_hl = t->level_hl;
    s_status.false_positives = fp;
    taskEXIT_CRITICAL(&s_lock);
}

// Espera silenciosa de duração aleatória. Apertos aqui são falsos positivos
// e reiniciam a espera. Retorna false se abortado.
static bool wait_gap(int *false_positives)
{
    uint32_t wait_ms = GAP_MIN_MS + esp_random() % GAP_RAND_MS;
    while (!s_abort) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms)) == 0)
            return true;
        if (s_abort)
            break;
        (*false_positives)++;
        wait_ms = GAP_AFTER_FP_MS + esp_random() % GAP_RAND_MS;
    }
    return false;
}

static void save_results(const aud_state_t *st, bool calibration)
{
    ha_audiogram_t result;
    aud_get_audiogram(st, &result);

    ha_settings_t *s;
    ha_audiogram_t *ag;
    app_lock(&s, &ag);
    if (calibration) {
        // O limiar médio de quem ouve normalmente define 0 dB HL.
        for (int e = 0; e < HA_NUM_EARS; e++)
            for (int b = 0; b < HA_NUM_BANDS; b++)
                if (ha_thr_is_measured(result.thr[e][b]))
                    s->cal_offset_db[e][b] += result.thr[e][b];
        ha_settings_sanitize(s);
    } else {
        *ag = result;
    }
    app_unlock();

    if (calibration)
        app_save_settings();
    else
        app_save_audiogram();

    for (int e = 0; e < HA_NUM_EARS; e++) {
        ESP_LOGI(TAG, "%s: 250=%d 500=%d 1k=%d 2k=%d 3k=%d 4k=%d 6k=%d 8k=%d (reteste 1k: dif %d dB)",
                 e == HA_EAR_LEFT ? "esquerdo" : "direito", result.thr[e][0], result.thr[e][1], result.thr[e][2],
                 result.thr[e][3], result.thr[e][4], result.thr[e][5], result.thr[e][6], result.thr[e][7],
                 aud_retest_diff(st, e));
    }
}

static void runner_task(void *arg)
{
    bool calibration = (bool)(intptr_t)arg;
    static aud_state_t st;

    ha_settings_t s;
    ha_settings_t *sp;
    app_lock(&sp, NULL);
    s = *sp;
    app_unlock();

    int max_level[HA_NUM_EARS][HA_NUM_BANDS];
    for (int e = 0; e < HA_NUM_EARS; e++)
        for (int b = 0; b < HA_NUM_BANDS; b++)
            max_level[e][b] = cal_max_hl(&s, e, b);

    const int ears[] = {HA_EAR_RIGHT, HA_EAR_LEFT};
    aud_init(&st, ears, 2, max_level);

    ESP_LOGI(TAG, "início (%s)", calibration ? "calibração biológica" : "audiometria");
    engine_set_mode(ENGINE_TONE);
    leds_blink(2, 200, 200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    ulTaskNotifyTake(pdTRUE, 0);

    int fp = 0;
    int responses = 0;
    int last_ear = -1;
    aud_trial_t t;
    while (!s_abort && aud_current_trial(&st, &t)) {
        set_status_trial(&st, &t, fp);
        // Com um LED só, o início de cada ouvido é sinalizado por piscadas
        // longas: 1 = direito, 2 = esquerdo.
        if (t.ear != last_ear) {
            last_ear = t.ear;
            leds_set(false);
            leds_blink(t.ear == HA_EAR_RIGHT ? 1 : 2, 400, 400);
            ulTaskNotifyTake(pdTRUE, 0); // apertos durante as piscadas não contam
        }
        leds_set(!s.led_during_tone);

        if (!wait_gap(&fp))
            break;

        float dbfs = cal_hl_to_dbfs(&s, t.ear, t.band, (float)t.level_hl);
        engine_play_tone(t.ear, ha_band_freqs[t.band], dbfs, TONE_PULSES, TONE_ON_MS, TONE_OFF_MS);
        if (s.led_during_tone)
            leds_set(true);

        bool heard = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TONE_TOTAL_MS + RESPONSE_EXTRA_MS)) > 0;
        if (s_abort)
            break;
        if (heard) {
            responses++;
            engine_stop_tone();
            leds_flash(150);
        }
        while (engine_tone_active())
            vTaskDelay(pdMS_TO_TICKS(10));
        if (s.led_during_tone)
            leds_set(false);

        ESP_LOGD(TAG, "ouvido=%d f=%.0f nivel=%d -> %s", t.ear, ha_band_freqs[t.band], t.level_hl,
                 heard ? "ouviu" : "não");
        aud_submit(&st, heard);
    }

    engine_stop_tone();
    // Nenhuma resposta em todo o teste indica que ninguém estava respondendo
    // (ou fone/DAC sem som). Salvar "sem resposta" em tudo prescreveria ganho
    // máximo em todas as bandas.
    bool completed = aud_done(&st) && responses > 0;
    if (completed)
        save_results(&st, calibration);
    else if (aud_done(&st))
        ESP_LOGW(TAG, "nenhuma resposta durante o teste; resultado descartado");
    else
        ESP_LOGW(TAG, "audiometria interrompida; nada foi salvo");

    app_apply_fitting();
    engine_set_mode(ENGINE_AID);
    leds_set(false);
    // Concluído: 3 piscadas rápidas. Cancelado: 1 piscada longa.
    if (completed)
        leds_blink(3, 150, 150);
    else
        leds_blink(1, 1000, 200);

    taskENTER_CRITICAL(&s_lock);
    s_status.running = false;
    s_status.last_completed = completed;
    s_status.step = st.step;
    s_status.false_positives = fp;
    for (int e = 0; e < HA_NUM_EARS; e++)
        s_status.retest_diff[e] = aud_retest_diff(&st, e);
    s_task = NULL;
    taskEXIT_CRITICAL(&s_lock);
    vTaskDelete(NULL);
}

bool audiometry_start(bool calibration)
{
    taskENTER_CRITICAL(&s_lock);
    if (s_status.running) {
        taskEXIT_CRITICAL(&s_lock);
        return false;
    }
    memset(&s_status, 0, sizeof(s_status));
    s_status.running = true;
    s_status.calibration = calibration;
    s_status.retest_diff[0] = s_status.retest_diff[1] = -1;
    s_abort = false;
    taskEXIT_CRITICAL(&s_lock);

    TaskHandle_t h;
    if (xTaskCreatePinnedToCore(runner_task, "audiometry", 4096, (void *)(intptr_t)calibration, 4, &h, 0) !=
        pdPASS) {
        s_status.running = false;
        return false;
    }
    taskENTER_CRITICAL(&s_lock);
    if (s_status.running)
        s_task = h;
    taskEXIT_CRITICAL(&s_lock);
    return true;
}

void audiometry_abort(void)
{
    s_abort = true;
    taskENTER_CRITICAL(&s_lock);
    TaskHandle_t h = s_task;
    taskEXIT_CRITICAL(&s_lock);
    if (h)
        xTaskNotifyGive(h);
}

bool audiometry_running(void)
{
    return s_status.running;
}

void audiometry_notify_press(void)
{
    taskENTER_CRITICAL(&s_lock);
    TaskHandle_t h = s_task;
    taskEXIT_CRITICAL(&s_lock);
    if (h)
        xTaskNotifyGive(h);
}

void audiometry_get_status(audiometry_status_t *st)
{
    taskENTER_CRITICAL(&s_lock);
    *st = s_status;
    taskEXIT_CRITICAL(&s_lock);
}
