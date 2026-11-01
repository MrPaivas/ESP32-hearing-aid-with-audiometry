#include "webui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "audio_engine.h"
#include "audiometry_runner.h"
#include "battery.h"
#include "board.h"
#include "cJSON.h"
#include "eq_fit.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "fitting.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "webui";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

#define MAX_BODY        2048
#define RESPONSE_POINTS 48
#define TEST_TONE_MS    1000
#define MAX_TEST_DBFS   (-6.0f)

static const char *ear_key[HA_NUM_EARS] = {"left", "right"};
static esp_timer_handle_t s_tone_timer;

// ---------- utilidades JSON ----------

static cJSON *thr_to_json(int8_t t)
{
    if (t == HA_THR_NOT_TESTED)
        return cJSON_CreateNull();
    if (t == HA_THR_NO_RESPONSE)
        return cJSON_CreateString("NR");
    return cJSON_CreateNumber(t);
}

static int8_t json_to_thr(const cJSON *v)
{
    if (cJSON_IsString(v) && strcmp(v->valuestring, "NR") == 0)
        return HA_THR_NO_RESPONSE;
    if (!cJSON_IsNumber(v))
        return HA_THR_NOT_TESTED;
    int x = (int)lround(v->valuedouble / HA_HL_STEP) * HA_HL_STEP;
    if (x < HA_HL_MIN)
        x = HA_HL_MIN;
    if (x > HA_HL_MAX)
        x = HA_HL_MAX;
    return (int8_t)x;
}

static cJSON *float_array(const float *v, int n)
{
    cJSON *a = cJSON_CreateArray();
    for (int i = 0; i < n; i++)
        cJSON_AddItemToArray(a, cJSON_CreateNumber(roundf(v[i] * 10.0f) / 10.0f));
    return a;
}

static cJSON *per_ear(float v[HA_NUM_EARS][HA_NUM_BANDS])
{
    cJSON *o = cJSON_CreateObject();
    for (int e = 0; e < HA_NUM_EARS; e++)
        cJSON_AddItemToObject(o, ear_key[e], float_array(v[e], HA_NUM_BANDS));
    return o;
}

static void read_per_ear(const cJSON *o, float v[HA_NUM_EARS][HA_NUM_BANDS])
{
    if (!cJSON_IsObject(o))
        return;
    for (int e = 0; e < HA_NUM_EARS; e++) {
        const cJSON *a = cJSON_GetObjectItem(o, ear_key[e]);
        for (int b = 0; b < HA_NUM_BANDS && cJSON_IsArray(a); b++) {
            const cJSON *x = cJSON_GetArrayItem(a, b);
            if (cJSON_IsNumber(x))
                v[e][b] = (float)x->valuedouble;
        }
    }
}

static void read_float(const cJSON *o, const char *key, float *dst)
{
    const cJSON *x = cJSON_GetObjectItem(o, key);
    if (cJSON_IsNumber(x))
        *dst = (float)x->valuedouble;
}

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *txt = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!txt)
        return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, txt);
    cJSON_free(txt);
    return err;
}

static esp_err_t send_ok(httpd_req_t *req)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", true);
    return send_json(req, r);
}

static cJSON *recv_json(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > MAX_BODY) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "corpo inválido");
        return NULL;
    }
    char buf[MAX_BODY + 1];
    int got = 0;
    while (got < (int)req->content_len) {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "falha na leitura");
            return NULL;
        }
        got += r;
    }
    buf[got] = '\0';
    cJSON *j = cJSON_Parse(buf);
    if (!j)
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON inválido");
    return j;
}

// ---------- handlers ----------

static esp_err_t h_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start - 1);
}

static esp_err_t h_state(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "freqs", float_array(ha_band_freqs, HA_NUM_BANDS));

    ha_settings_t s;
    ha_audiogram_t ag;
    ha_settings_t *sp;
    ha_audiogram_t *agp;
    app_lock(&sp, &agp);
    s = *sp;
    ag = *agp;
    app_unlock();

    cJSON *jag = cJSON_AddObjectToObject(root, "audiogram");
    for (int e = 0; e < HA_NUM_EARS; e++) {
        cJSON *a = cJSON_AddArrayToObject(jag, ear_key[e]);
        for (int b = 0; b < HA_NUM_BANDS; b++)
            cJSON_AddItemToArray(a, thr_to_json(ag.thr[e][b]));
    }

    // Alvo NAL-R e resposta real do equalizador ajustado.
    float targets[HA_NUM_EARS][HA_NUM_BANDS];
    fitting_targets(&ag, &s, targets);
    cJSON_AddItemToObject(root, "targets", per_ear(targets));

    ha_dsp_params_t p;
    fitting_build_params(&ag, &s, false, &p);
    cJSON *resp = cJSON_AddObjectToObject(root, "response");
    float rf[RESPONSE_POINTS];
    for (int i = 0; i < RESPONSE_POINTS; i++)
        rf[i] = 125.0f * powf(2.0f, 7.0f * (float)i / (RESPONSE_POINTS - 1)); // 125 Hz .. 16 kHz
    for (int i = 0; i < RESPONSE_POINTS; i++)
        if (rf[i] > HA_SAMPLE_RATE * 0.45f)
            rf[i] = HA_SAMPLE_RATE * 0.45f;
    cJSON_AddItemToObject(resp, "freqs", float_array(rf, RESPONSE_POINTS));
    for (int e = 0; e < HA_NUM_EARS; e++) {
        float r[RESPONSE_POINTS];
        for (int i = 0; i < RESPONSE_POINTS; i++)
            r[i] = eq_response_db(p.eq_gain_db[e], HA_SAMPLE_RATE, rf[i]);
        cJSON_AddItemToObject(resp, ear_key[e], float_array(r, RESPONSE_POINTS));
    }

    cJSON *js = cJSON_AddObjectToObject(root, "settings");
    cJSON_AddNumberToObject(js, "volume_db", s.volume_db);
    cJSON_AddNumberToObject(js, "volume_max_db", s.volume_max_db);
    cJSON_AddNumberToObject(js, "max_gain_db", s.max_gain_db);
    cJSON_AddNumberToObject(js, "mpo_spl", s.mpo_spl);
    cJSON_AddNumberToObject(js, "wdrc_knee_spl", s.wdrc_knee_spl);
    cJSON_AddNumberToObject(js, "wdrc_ratio", s.wdrc_ratio);
    cJSON_AddNumberToObject(js, "mic_spl_at_0dbfs", s.mic_spl_at_0dbfs);
    cJSON_AddNumberToObject(js, "out_spl_at_0dbfs", s.out_spl_at_0dbfs);
    cJSON_AddNumberToObject(js, "transducer", s.transducer);
    cJSON_AddNumberToObject(js, "max_test_hl", s.max_test_hl);
    cJSON_AddBoolToObject(js, "led_during_tone", s.led_during_tone);
    cJSON_AddItemToObject(js, "fine_tune_db", per_ear(s.fine_tune_db));
    cJSON_AddItemToObject(js, "cal_offset_db", per_ear(s.cal_offset_db));

    cJSON_AddBoolToObject(root, "muted", app_is_muted());

    audiometry_status_t st;
    audiometry_get_status(&st);
    cJSON *ja = cJSON_AddObjectToObject(root, "audiometry");
    cJSON_AddBoolToObject(ja, "running", st.running);
    cJSON_AddBoolToObject(ja, "calibration", st.calibration);
    cJSON_AddBoolToObject(ja, "last_completed", st.last_completed);
    cJSON_AddNumberToObject(ja, "step", st.step);
    cJSON_AddNumberToObject(ja, "total_steps", st.total_steps);
    cJSON_AddStringToObject(ja, "ear", ear_key[st.ear]);
    cJSON_AddNumberToObject(ja, "freq", ha_band_freqs[st.band]);
    cJSON_AddNumberToObject(ja, "level_hl", st.level_hl);
    cJSON_AddNumberToObject(ja, "false_positives", st.false_positives);
    cJSON *rd = cJSON_AddObjectToObject(ja, "retest_diff");
    for (int e = 0; e < HA_NUM_EARS; e++)
        cJSON_AddNumberToObject(rd, ear_key[e], st.retest_diff[e]);

    float v = battery_voltage();
    cJSON *jb = cJSON_AddObjectToObject(root, "battery");
    cJSON_AddNumberToObject(jb, "voltage", roundf(v * 100.0f) / 100.0f);
    cJSON_AddNumberToObject(jb, "percent", battery_percent(v));

    engine_stats_t es;
    engine_get_stats(&es);
    cJSON *je = cJSON_AddObjectToObject(root, "engine");
    cJSON_AddNumberToObject(je, "proc_us_avg", es.proc_us_avg);
    cJSON_AddNumberToObject(je, "proc_us_max", es.proc_us_max);
    cJSON_AddNumberToObject(je, "block_us", 1000000.0 * HA_BLOCK_SIZE / HA_SAMPLE_RATE);
    cJSON_AddItemToObject(je, "mic_rms_dbfs", float_array(es.mic_rms_dbfs, 2));

    return send_json(req, root);
}

static esp_err_t h_settings(httpd_req_t *req)
{
    cJSON *j = recv_json(req);
    if (!j)
        return ESP_FAIL;

    ha_settings_t *s;
    app_lock(&s, NULL);
    read_float(j, "volume_db", &s->volume_db);
    read_float(j, "volume_max_db", &s->volume_max_db);
    read_float(j, "max_gain_db", &s->max_gain_db);
    read_float(j, "mpo_spl", &s->mpo_spl);
    read_float(j, "wdrc_knee_spl", &s->wdrc_knee_spl);
    read_float(j, "wdrc_ratio", &s->wdrc_ratio);
    read_float(j, "mic_spl_at_0dbfs", &s->mic_spl_at_0dbfs);
    read_float(j, "out_spl_at_0dbfs", &s->out_spl_at_0dbfs);
    read_float(j, "max_test_hl", &s->max_test_hl);
    const cJSON *x = cJSON_GetObjectItem(j, "transducer");
    if (cJSON_IsNumber(x))
        s->transducer = (uint8_t)x->valueint;
    x = cJSON_GetObjectItem(j, "led_during_tone");
    if (cJSON_IsBool(x))
        s->led_during_tone = cJSON_IsTrue(x);
    read_per_ear(cJSON_GetObjectItem(j, "fine_tune_db"), s->fine_tune_db);
    read_per_ear(cJSON_GetObjectItem(j, "cal_offset_db"), s->cal_offset_db);
    ha_settings_sanitize(s);
    app_unlock();

    x = cJSON_GetObjectItem(j, "muted");
    if (cJSON_IsBool(x))
        app_set_mute(cJSON_IsTrue(x));
    cJSON_Delete(j);

    app_apply_fitting();
    app_save_settings();
    return send_ok(req);
}

static esp_err_t h_audiogram(httpd_req_t *req)
{
    cJSON *j = recv_json(req);
    if (!j)
        return ESP_FAIL;
    ha_audiogram_t *ag;
    app_lock(NULL, &ag);
    for (int e = 0; e < HA_NUM_EARS; e++) {
        const cJSON *a = cJSON_GetObjectItem(j, ear_key[e]);
        if (!cJSON_IsArray(a))
            continue;
        for (int b = 0; b < HA_NUM_BANDS; b++)
            ag->thr[e][b] = json_to_thr(cJSON_GetArrayItem(a, b));
    }
    app_unlock();
    cJSON_Delete(j);
    app_save_audiogram();
    app_apply_fitting();
    return send_ok(req);
}

static esp_err_t h_audiometry(httpd_req_t *req)
{
    cJSON *j = recv_json(req);
    if (!j)
        return ESP_FAIL;
    const cJSON *action = cJSON_GetObjectItem(j, "action");
    bool calibration = cJSON_IsTrue(cJSON_GetObjectItem(j, "calibration"));
    bool ok = true;
    if (cJSON_IsString(action) && strcmp(action->valuestring, "start") == 0)
        ok = audiometry_start(calibration);
    else
        audiometry_abort();
    cJSON_Delete(j);
    if (!ok)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "audiometria já em andamento");
    return send_ok(req);
}

static void tone_timer_cb(void *arg)
{
    if (!audiometry_running())
        engine_set_mode(ENGINE_AID);
}

// Tom de teste: {"ear":"left","band":2,"hl":40} ou {"ear":"left","freq":1000,"dbfs":-20}
static esp_err_t h_tone(httpd_req_t *req)
{
    if (audiometry_running())
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "audiometria em andamento");
    cJSON *j = recv_json(req);
    if (!j)
        return ESP_FAIL;
    const cJSON *je = cJSON_GetObjectItem(j, "ear");
    int ear = cJSON_IsString(je) && strcmp(je->valuestring, "left") == 0 ? HA_EAR_LEFT : HA_EAR_RIGHT;

    float freq, dbfs;
    const cJSON *jd = cJSON_GetObjectItem(j, "dbfs");
    if (cJSON_IsNumber(jd)) {
        const cJSON *jf = cJSON_GetObjectItem(j, "freq");
        freq = cJSON_IsNumber(jf) ? (float)jf->valuedouble : 1000.0f;
        dbfs = (float)jd->valuedouble;
    } else {
        const cJSON *jb = cJSON_GetObjectItem(j, "band");
        const cJSON *jh = cJSON_GetObjectItem(j, "hl");
        int band = cJSON_IsNumber(jb) ? jb->valueint : BAND_1K;
        if (band < 0 || band >= HA_NUM_BANDS)
            band = BAND_1K;
        ha_settings_t *s;
        app_lock(&s, NULL);
        float hl = cJSON_IsNumber(jh) ? (float)jh->valuedouble : 40.0f;
        if (hl > s->max_test_hl)
            hl = s->max_test_hl;
        dbfs = cal_hl_to_dbfs(s, ear, band, hl);
        app_unlock();
        freq = ha_band_freqs[band];
    }
    cJSON_Delete(j);

    if (freq < 100.0f || freq > 12000.0f)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "frequência fora da faixa");
    if (dbfs > MAX_TEST_DBFS)
        dbfs = MAX_TEST_DBFS;

    engine_set_mode(ENGINE_TONE);
    engine_play_tone(ear, freq, dbfs, 1, TEST_TONE_MS, 0);
    esp_timer_stop(s_tone_timer);
    esp_timer_start_once(s_tone_timer, (TEST_TONE_MS + 200) * 1000ULL);
    return send_ok(req);
}

static esp_err_t h_cal_reset(httpd_req_t *req)
{
    ha_settings_t *s;
    app_lock(&s, NULL);
    memset(s->cal_offset_db, 0, sizeof(s->cal_offset_db));
    app_unlock();
    app_save_settings();
    app_apply_fitting();
    return send_ok(req);
}

static esp_err_t h_restart(httpd_req_t *req)
{
    send_ok(req);
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}

// ---------- inicialização ----------

static void wifi_ap_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    wifi_config_t wc = {0};
    snprintf((char *)wc.ap.ssid, sizeof(wc.ap.ssid), "HearingAid-%02X%02X", mac[4], mac[5]);
    wc.ap.ssid_len = strlen((char *)wc.ap.ssid);
    strlcpy((char *)wc.ap.password, WIFI_AP_PASSWORD, sizeof(wc.ap.password));
    wc.ap.channel = 1;
    wc.ap.max_connection = 2;
    wc.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi '%s' senha '%s' -> http://192.168.4.1", wc.ap.ssid, WIFI_AP_PASSWORD);
}

void webui_start(void)
{
    wifi_ap_start();

    esp_timer_create_args_t targs = {.callback = tone_timer_cb, .name = "testtone"};
    esp_timer_create(&targs, &s_tone_timer);

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 12;
    cfg.stack_size = 8192; // buffers de JSON/corpo na pilha
    cfg.core_id = 0;
    httpd_handle_t srv;
    ESP_ERROR_CHECK(httpd_start(&srv, &cfg));

    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = h_index},
        {.uri = "/api/state", .method = HTTP_GET, .handler = h_state},
        {.uri = "/api/settings", .method = HTTP_POST, .handler = h_settings},
        {.uri = "/api/audiogram", .method = HTTP_POST, .handler = h_audiogram},
        {.uri = "/api/audiometry", .method = HTTP_POST, .handler = h_audiometry},
        {.uri = "/api/tone", .method = HTTP_POST, .handler = h_tone},
        {.uri = "/api/calibration/reset", .method = HTTP_POST, .handler = h_cal_reset},
        {.uri = "/api/restart", .method = HTTP_POST, .handler = h_restart},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        httpd_register_uri_handler(srv, &uris[i]);
}
