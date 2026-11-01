# Hardware

[Português](pt-BR/hardware.md)

> **Warning:** this project is experimental and **is not a medical device**. Read [SAFETY.md](../SAFETY.md) before building it.

## Wiring

| Module | Module pin | ESP32 |
|---|---|---|
| **INMP441** | VDD | 3V3 |
| | GND | GND |
| | SCK | GPIO26 |
| | WS | GPIO25 |
| | SD | GPIO33 |
| | L/R | GND (left channel; see `MIC_SLOT` in `src/board.h`) |
| **PCM5102A** | VIN / VCC | Battery after the switch (TP4056 OUT+) if the module has a regulator; otherwise 3V3. **Do not use the ESP32 VIN pin**: it only has 5 V with USB connected |
| | GND | GND |
| | BCK | GPIO26 |
| | LCK / LRCK | GPIO25 |
| | DIN | GPIO22 |
| | SCK | GND (uses the internal PLL from BCK) |
| | FMT | GND (I2S format) |
| | FLT | GND |
| | DEMP | GND |
| | XSMT | GPIO21 (muted at boot to avoid a pop) |
| **Encoder** | A (CLK) | GPIO32 |
| | B (DT) | GPIO27 |
| | Common | GND |
| | Button | GPIO13 (other terminal to GND) |
| **LED** | The board's blue LED (D2) | GPIO2 (nothing external) |
| **Battery** | + through a 100 kΩ / 100 kΩ divider | GPIO34 (divider midpoint) |

The microphone and DAC share BCLK/WS on the same I2S peripheral (full-duplex). This removes clock drift between input and output and keeps latency fixed.

### Salvaged encoder

- Mechanical radio encoders usually have 3 pins (A, common, B) plus 2 for the button. Find the common pin with a multimeter: it has alternating continuity with A and with B as you turn.
- Internal pull-ups are used. If the encoder is noisy, add 10 kΩ to 3V3 and 10 nF to GND on A and B.
- If each click changes the volume twice (or not at all), adjust `ENC_STEPS_PER_DETENT` (4 or 2) in `src/board.h`. If the direction is reversed, swap A and B.

### Microphone: left or right?

Every 10 s, the serial log shows the level of each I2S slot (`mic slot0 ... slot1 ...`). The slot with the higher level (for example, −60 dBFS in a quiet room, rising when you speak) is the microphone. If it is slot 1, set `MIC_SLOT` to 1.

## Power

```
18650 ──► TP4056 with protection (DW01) ──► on/off switch ──► 3.3 V LDO ──► ESP32 3V3 + modules
                                                            ├─► PCM5102A module VIN (own regulator)
                                                            └─► 100k/100k divider ──► GPIO34
```

- The external LDO feeds the ESP32 DevKit **3.3V** pin, not VIN. VIN goes through the board's AMS1117, which needs about 4.4 V at its input, more than an 18650 supplies.
- The DevKit VIN pin only has 5 V when USB is connected. On battery, the circuit has no 5 V.
- Do not connect USB and the battery at the same time while the external LDO feeds the 3.3V pin, unless you add a diode or switch. The board's regulator and the external one would be in parallel.
- Use a **low-dropout regulator** (ME6211, HT7333, RT9013; MCP1700 only with care, because of Wi-Fi peak current). The AMS1117 needs about 1.1 V of dropout and cuts out early on an 18650.
- Expected typical consumption: about 50–70 mA in hearing aid mode (Wi-Fi off), about 150 mA in configuration mode.
- With the battery monitor enabled (`BATT_MONITOR_ENABLED` set to 1), the LED blinks 3 times every minute below 3.40 V, and below 3.20 V the device enters deep sleep (turn it off and recharge).
- Place 100 µF + 100 nF near the ESP32 and 10 µF + 100 nF near each I2S module. Power supply noise shows up as an amplified hum.

## Headphones

- Prefer **closed** headphones (supra-aural, or in-ear with a good seal). Open headphones let the output reach the microphone and cause feedback (whistling).
- Keep the microphone away from the headphones.
- Set the headphone type on the configuration page (it selects the RETSPL table used in audiometry).

## Calibration

Audiometry measures in dB HL, which depends on the headphone's sensitivity. Without lab equipment, there are two options (web page → "Tom de teste e calibração", the page is in Portuguese):

1. **Biological calibration (recommended):** a young person with normal hearing runs the audiometry with the "Calibração biológica" button. Their thresholds become 0 dB HL for those headphones. Repeat if you change headphones.
2. **Reference tone:** play the 1 kHz tone at −20 dBFS, measure the SPL at the headphone with a sound level meter or coupler, and enter `measured value + 20` under "SPL de saída a 0 dBFS" (output SPL at 0 dBFS).

For controls and usage, see the [README](../README.md#usage).
