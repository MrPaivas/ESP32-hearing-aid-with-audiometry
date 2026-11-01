// Encoder rotativo (PCNT em quadratura) e botão com clique/duplo/longo.
#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    INPUT_ROTATE,   // value = detentes (+ horário)
    INPUT_BTN_DOWN, // imediato ao apertar (resposta da audiometria)
    INPUT_CLICK,
    INPUT_DOUBLE_CLICK,
    INPUT_LONG_PRESS, // emitido ainda segurando, após 3 s
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    int value;
} input_event_t;

void input_init(void);
QueueHandle_t input_queue(void);
// Leitura direta (usada no boot para detectar o modo configuração).
bool input_button_pressed(void);
