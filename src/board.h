// Pinagem e parâmetros da placa. Ver docs/hardware.md.
#pragma once

#include "driver/gpio.h"

// I2S0 full-duplex: INMP441 (entrada) e PCM5102A (saída) compartilham BCLK/WS.
#define PIN_I2S_BCLK GPIO_NUM_26
#define PIN_I2S_WS   GPIO_NUM_25
#define PIN_I2S_DIN  GPIO_NUM_33 // INMP441 SD
#define PIN_I2S_DOUT GPIO_NUM_22 // PCM5102A DIN
#define PIN_DAC_XSMT GPIO_NUM_21 // PCM5102A soft-mute (alto = som liberado)

// Slot do INMP441 no quadro estéreo: 0 com o pino L/R em GND, 1 com L/R em VDD.
#define MIC_SLOT 0

// Mais descritores = mais tolerância a atrasos, porém mais latência (1 ms cada).
#define I2S_DMA_DESC_NUM 3

#define PIN_ENC_A   GPIO_NUM_32
#define PIN_ENC_B   GPIO_NUM_27
#define PIN_ENC_BTN GPIO_NUM_13
// Transições de quadratura por "clique" do encoder (4 na maioria; alguns são 2).
#define ENC_STEPS_PER_DETENT 4

// LED azul da própria placa (D2). GPIO2 é pino de boot: não ligue nada que o
// puxe para alto.
#define PIN_LED GPIO_NUM_2

// Divisor 100k/100k da bateria para o ADC1_CH6. Deixe 0 se o divisor não
// estiver montado: com o pino flutuando a leitura é aleatória e poderia
// desligar o aparelho por "bateria crítica".
#define BATT_MONITOR_ENABLED 0
#define PIN_BATT_ADC      GPIO_NUM_34
#define BATT_DIVIDER      2.0f
#define BATT_LOW_V        3.40f
#define BATT_CRITICAL_V   3.20f

#define WIFI_AP_PASSWORD "aparelho123"
