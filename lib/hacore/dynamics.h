// Compressor WDRC broadband (ganho calculado por bloco sobre o nível de
// entrada) e limitador de saída (MPO) por amostra.
#pragma once

typedef struct {
    float knee_spl;          // nível de entrada (dB SPL) onde começa a compressão
    float ratio;             // ex.: 2.0 = 2:1
    float spl_at_0dbfs;      // calibração do microfone
    float attack_coef;       // suavização por bloco
    float release_coef;
    float gain_db;           // ganho atual (<= 0)
} wdrc_t;

void wdrc_init(wdrc_t *c, float block_rate_hz, float attack_ms, float release_ms);
void wdrc_set(wdrc_t *c, float knee_spl, float ratio, float spl_at_0dbfs);
// Atualiza com um bloco de entrada e retorna o ganho linear alvo do bloco.
float wdrc_block(wdrc_t *c, const float *in, int n);

typedef struct {
    float ceiling;      // amplitude linear máxima
    float release_coef; // por amostra
    float gain;
} limiter_t;

void limiter_init(limiter_t *l, float fs, float release_ms);
void limiter_set_ceiling_db(limiter_t *l, float ceiling_dbfs);
void limiter_process(limiter_t *l, float *buf, int n);
