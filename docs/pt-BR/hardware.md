# Hardware

[English](../hardware.md)

> **Aviso:** este projeto é experimental e **não é um dispositivo médico**. Leia o [SAFETY.pt-BR.md](../../SAFETY.pt-BR.md) antes de montar.

## Ligações

| Módulo | Pino do módulo | ESP32 |
|---|---|---|
| **INMP441** | VDD | 3V3 |
| | GND | GND |
| | SCK | GPIO26 |
| | WS | GPIO25 |
| | SD | GPIO33 |
| | L/R | GND (canal esquerdo; ver `MIC_SLOT` em `src/board.h`) |
| **PCM5102A** | VIN / VCC | Bateria após a chave (TP4056 OUT+), se o módulo tiver regulador; senão, 3V3. **Não use o pino VIN do ESP32**: ele só tem 5 V com o USB conectado |
| | GND | GND |
| | BCK | GPIO26 |
| | LCK / LRCK | GPIO25 |
| | DIN | GPIO22 |
| | SCK | GND (usa PLL interno a partir do BCK) |
| | FMT | GND (formato I2S) |
| | FLT | GND |
| | DEMP | GND |
| | XSMT | GPIO21 (mudo no boot, evita estalo) |
| **Encoder** | A (CLK) | GPIO32 |
| | B (DT) | GPIO27 |
| | Comum | GND |
| | Botão | GPIO13 (outro terminal em GND) |
| **LED** | LED azul da própria placa (D2) | GPIO2 (nada externo) |
| **Bateria** | + via divisor 100 kΩ / 100 kΩ | GPIO34 (ponto central do divisor) |

Microfone e DAC compartilham BCLK/WS no mesmo periférico I2S (full-duplex). Isso elimina deriva de clock entre entrada e saída e mantém a latência fixa.

### Encoder de sucata
- Encoders mecânicos de rádio geralmente têm 3 pinos (A, comum, B) + 2 do botão. Identifique o comum com um multímetro: ele tem continuidade alternada com A e com B ao girar.
- Os pull-ups internos são usados; se o encoder for ruidoso, adicione 10 kΩ para 3V3 e 10 nF para GND em A e B.
- Se cada "clique" mudar o volume 2 vezes (ou nenhuma), ajuste `ENC_STEPS_PER_DETENT` (4 ou 2) em `src/board.h`. Se o sentido estiver invertido, troque A e B.

### Microfone: esquerdo ou direito?
O log serial mostra a cada 10 s o nível de cada slot I2S (`mic slot0 ... slot1 ...`). O slot com nível mais alto (ex.: −60 dBFS em sala silenciosa, sobe ao falar) é o do microfone. Se for o slot 1, mude `MIC_SLOT` para 1.

## Alimentação

```
18650 ──► TP4056 com proteção (DW01) ──► chave liga/desliga ──► LDO 3,3 V ──► 3V3 do ESP32 + módulos
                                                              ├─► VIN do módulo PCM5102A (regulador próprio)
                                                              └─► divisor 100k/100k ──► GPIO34
```

- O LDO externo alimenta o pino **3,3V** do ESP32 DevKit, e não o VIN. O VIN passa pelo AMS1117 da placa, que precisa de cerca de 4,4 V na entrada, mais do que a 18650 fornece.
- O pino VIN do DevKit só tem 5 V quando o USB está conectado. Com bateria, não existe 5 V no circuito.
- Não ligue o USB e a bateria ao mesmo tempo com o LDO externo alimentando o 3,3V sem um diodo ou chave. O regulador da placa e o externo ficariam em paralelo.

- Use um regulador **LDO de baixo dropout** (ME6211, HT7333, RT9013, MCP1700 com cuidado com a corrente de pico do Wi-Fi). O AMS1117 precisa de ~1,1 V de dropout e desliga cedo com a 18650.
- Consumo típico esperado: ~50–70 mA no modo aparelho (Wi-Fi desligado), ~150 mA no modo configuração.
- Com o monitor de bateria ativado (`BATT_MONITOR_ENABLED` em 1), abaixo de 3,40 V o LED pisca 3 vezes a cada minuto e abaixo de 3,20 V o aparelho entra em deep sleep (desligue e recarregue).
- Coloque 100 µF + 100 nF perto do ESP32 e 10 µF + 100 nF perto de cada módulo I2S; ruído de alimentação aparece como zumbido amplificado.

## Fones

- Prefira fones **fechados** (supra-aurais ou intra-auriculares com boa vedação). Fones abertos deixam o som da saída voltar ao microfone e causam microfonia (apito).
- Mantenha o microfone longe dos fones.
- Informe o tipo de fone na página de configuração (tabela RETSPL usada na audiometria).

## Calibração

A audiometria mede em dB HL, que depende da sensibilidade do fone. Sem equipamento de laboratório há duas opções (página web → "Tom de teste e calibração"):

1. **Calibração biológica (recomendada):** uma pessoa jovem com audição normal faz a audiometria em "Calibração biológica". Os limiares dela passam a ser o 0 dB HL daquele fone. Refazer se trocar de fone.
2. **Tom de referência:** toque o tom de 1 kHz a −20 dBFS, meça o SPL no fone com um decibelímetro/acoplador e informe `valor medido + 20` em "SPL de saída a 0 dBFS".

Para controles e uso, veja o [README](../../README.pt-BR.md#como-usar).
