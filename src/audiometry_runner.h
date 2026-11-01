// Executa a audiometria no hardware: temporização dos tons, janela de
// resposta, falsos positivos e gravação do resultado.
#pragma once

#include <stdbool.h>

typedef struct {
    bool running;
    bool calibration;
    bool last_completed; // o último teste terminou (não foi abortado)
    int step, total_steps;
    int ear, band, level_hl;
    int false_positives;
    int retest_diff[2]; // |1k teste - 1k reteste| por ouvido, -1 se indisponível
} audiometry_status_t;

// calibration = true: calibração biológica (pessoa com audição normal); o
// resultado vira o 0 dB HL de referência em vez de ser salvo como audiograma.
bool audiometry_start(bool calibration);
void audiometry_abort(void);
bool audiometry_running(void);
// Chamado a cada aperto do botão.
void audiometry_notify_press(void);
void audiometry_get_status(audiometry_status_t *st);
