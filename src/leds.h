#pragma once

#include <stdbool.h>

// LED único da placa (D2 do DevKit, GPIO2).
void leds_init(void);
// Estado "base" do LED (ex.: audiometria em andamento).
void leds_set(bool on);
// Inverte o LED por ms e volta ao estado base (visível com o LED aceso ou apagado).
void leds_flash(int ms);
// Pisca n vezes (bloqueante).
void leds_blink(int times, int on_ms, int off_ms);
