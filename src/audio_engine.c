#include "audio_engine.h"

#include <math.h>
#include <string.h>

#include "audio_io.h"
#include "board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha_types.h"
#include "tone_gen.h"

static const char *TAG = "engine";

#define TONE_RAMP_MS  20.0f
#define WARMUP_BLOCKS 100 // ~100 ms mudo no boot para evitar estalo
#define STATS_BLOCKS  (HA_SAMPLE_RATE / HA_BLOCK_SIZE)

typedef struct {
    bool mode_dirty;
    engine_mode_t mode;
    bool params_dirty;
    ha_dsp_params_t params;
    bool tone_start;
    int tone_ear;
    float tone_freq, tone_amp;
    int tone_pulses;
    float tone_on_ms, tone_off_ms;
    bool tone_stop;
} pending_t;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static pending_t s_pending;
static volatile engine_mode_t s_mode = ENGINE_SILENT;
static volatile bool s_tone_busy;
static engine_stats_t s_stats;

// Estado do task de áudio (acessado só por ele).
static ha_dsp_t s_dsp;
static ha_dsp_params_t s_params;
static bool s_have_params;
static tone_gen_t s_tone;
static int s_tone_ear;

static void apply_pending(void)
{
    pending_t p;
    taskENTER_CRITICAL(&s_lock);
    p = s_pending;
    s_pending.mode_dirty = s_pending.params_dirty = s_pending.tone_start = s_pending.tone_stop = false;
    taskEXIT_CRITICAL(&s_lock);

    if (p.params_dirty) {
        s_params = p.params;
        s_have_params = true;
        ha_dsp_configure(&s_dsp, &s_params);
    }
    if (p.mode_dirty && p.mode != s_mode) {
        if (p.mode == ENGINE_AID) {
            // Reinicia estados; o ganho sobe em rampa a partir de zero.
            ha_dsp_init(&s_dsp, HA_SAMPLE_RATE);
            if (s_have_params)
                ha_dsp_configure(&s_dsp, &s_params);
        }
        if (p.mode != ENGINE_TONE) {
            s_tone.active = false;
            s_tone_busy = false;
        }
        s_mode = p.mode;
    }
    if (p.tone_start) {
        s_tone_ear = p.tone_ear;
        tone_gen_start(&s_tone, HA_SAMPLE_RATE, p.tone_freq, p.tone_amp, p.tone_pulses, p.tone_on_ms,
                       p.tone_off_ms, TONE_RAMP_MS);
    }
    if (p.tone_stop)
        tone_gen_stop(&s_tone);
}

static inline int32_t to_i32(float x)
{
    if (x >= 1.0f)
        return INT32_MAX;
    if (x <= -1.0f)
        return INT32_MIN;
    return (int32_t)(x * 2147483648.0f);
}

static void engine_task(void *arg)
{
    static int32_t io[HA_BLOCK_SIZE * 2];
    static float in[HA_BLOCK_SIZE], out_l[HA_BLOCK_SIZE], out_r[HA_BLOCK_SIZE];
    int warmup = WARMUP_BLOCKS;
    int stat_blocks = 0;
    uint64_t proc_sum = 0;
    uint32_t proc_max = 0;
    float slot_energy[2] = {0, 0};

    ha_dsp_init(&s_dsp, HA_SAMPLE_RATE);

    for (;;) {
        audio_io_read(io, HA_BLOCK_SIZE);
        int64_t t0 = esp_timer_get_time();

        apply_pending();

        const float scale = 1.0f / 2147483648.0f;
        for (int i = 0; i < HA_BLOCK_SIZE; i++) {
            float s0 = (float)io[2 * i] * scale;
            float s1 = (float)io[2 * i + 1] * scale;
            slot_energy[0] += s0 * s0;
            slot_energy[1] += s1 * s1;
            in[i] = MIC_SLOT == 0 ? s0 : s1;
        }

        switch (s_mode) {
        case ENGINE_AID:
            ha_dsp_process(&s_dsp, in, out_l, out_r, HA_BLOCK_SIZE);
            break;
        case ENGINE_TONE: {
            float *on = s_tone_ear == HA_EAR_LEFT ? out_l : out_r;
            float *off = s_tone_ear == HA_EAR_LEFT ? out_r : out_l;
            tone_gen_render(&s_tone, on, HA_BLOCK_SIZE);
            memset(off, 0, sizeof(out_l));
            if (!tone_gen_active(&s_tone))
                s_tone_busy = false;
            break;
        }
        default:
            memset(out_l, 0, sizeof(out_l));
            memset(out_r, 0, sizeof(out_r));
            break;
        }

        for (int i = 0; i < HA_BLOCK_SIZE; i++) {
            io[2 * i] = to_i32(out_l[i]);
            io[2 * i + 1] = to_i32(out_r[i]);
        }

        uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);
        proc_sum += dt;
        if (dt > proc_max)
            proc_max = dt;
        if (++stat_blocks >= STATS_BLOCKS) {
            float n = (float)(stat_blocks * HA_BLOCK_SIZE);
            taskENTER_CRITICAL(&s_lock);
            s_stats.proc_us_avg = (uint32_t)(proc_sum / stat_blocks);
            s_stats.proc_us_max = proc_max;
            for (int k = 0; k < 2; k++)
                s_stats.mic_rms_dbfs[k] = 10.0f * log10f(slot_energy[k] / n + 1e-20f);
            taskEXIT_CRITICAL(&s_lock);
            stat_blocks = 0;
            proc_sum = 0;
            proc_max = 0;
            slot_energy[0] = slot_energy[1] = 0;
        }

        audio_io_write(io, HA_BLOCK_SIZE);

        if (warmup > 0 && --warmup == 0)
            audio_io_mute(false);
    }
}

void engine_start(void)
{
    ESP_ERROR_CHECK(audio_io_init(HA_SAMPLE_RATE));
    // Prioridade máxima no core 1; Wi-Fi e HTTP ficam no core 0.
    xTaskCreatePinnedToCore(engine_task, "audio", 6144, NULL, configMAX_PRIORITIES - 1, NULL, 1);
    ESP_LOGI(TAG, "fs=%d Hz, bloco=%d, DMA=%d x %d quadros", HA_SAMPLE_RATE, HA_BLOCK_SIZE, I2S_DMA_DESC_NUM,
             HA_BLOCK_SIZE);
}

void engine_set_mode(engine_mode_t mode)
{
    taskENTER_CRITICAL(&s_lock);
    s_pending.mode = mode;
    s_pending.mode_dirty = true;
    if (mode != ENGINE_TONE)
        s_pending.tone_start = false;
    taskEXIT_CRITICAL(&s_lock);
}

engine_mode_t engine_get_mode(void)
{
    return s_mode;
}

void engine_set_aid_params(const ha_dsp_params_t *p)
{
    taskENTER_CRITICAL(&s_lock);
    s_pending.params = *p;
    s_pending.params_dirty = true;
    taskEXIT_CRITICAL(&s_lock);
}

void engine_play_tone(int ear, float freq, float amp_dbfs, int pulses, float on_ms, float off_ms)
{
    if (amp_dbfs > 0.0f)
        amp_dbfs = 0.0f;
    taskENTER_CRITICAL(&s_lock);
    s_pending.tone_start = true;
    s_pending.tone_stop = false;
    s_pending.tone_ear = ear;
    s_pending.tone_freq = freq;
    s_pending.tone_amp = powf(10.0f, amp_dbfs / 20.0f);
    s_pending.tone_pulses = pulses;
    s_pending.tone_on_ms = on_ms;
    s_pending.tone_off_ms = off_ms;
    s_tone_busy = true;
    taskEXIT_CRITICAL(&s_lock);
}

void engine_stop_tone(void)
{
    taskENTER_CRITICAL(&s_lock);
    s_pending.tone_stop = true;
    taskEXIT_CRITICAL(&s_lock);
}

bool engine_tone_active(void)
{
    return s_tone_busy;
}

void engine_get_stats(engine_stats_t *st)
{
    taskENTER_CRITICAL(&s_lock);
    *st = s_stats;
    taskEXIT_CRITICAL(&s_lock);
}
