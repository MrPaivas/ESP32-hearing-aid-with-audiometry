# ESP32 Hearing Aid with Audiometry

[![CI](https://github.com/MrPaivas/ESP32-hearing-aid-with-audiometry/actions/workflows/ci.yml/badge.svg)](https://github.com/MrPaivas/ESP32-hearing-aid-with-audiometry/actions/workflows/ci.yml)
[![License: Apache-2.0](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

[Português](README.pt-BR.md)

An experimental hearing aid built on an ESP32-WROOM-32, an INMP441 I2S microphone and a PCM5102A I2S DAC with a 3.5 mm output, with built-in pure-tone audiometry.

> [!WARNING]
> **This is not a medical device.** It plays amplified sound directly into your ears and can cause permanent hearing damage. Read [SAFETY.md](SAFETY.md) before building or using it.

## Why this project exists

The goal is to explore how far low-cost parts can go as a hearing aid: the device measures which frequencies you have trouble hearing in each ear, then amplifies ambient sound to match, with as little delay as possible.

## Status and limitations

This is a breadboard prototype.

- The latency of 4–5 ms is an estimate, not a measurement.
- It has no clinical validation.
- The battery monitor is off by default (`BATT_MONITOR_ENABLED` is 0 in `src/board.h`).
- No PCB is published yet.
- The default Wi-Fi password is public; see [SECURITY.md](SECURITY.md).

## Features

- **Pure-tone audiometry per ear:** modified Hughson-Westlake procedure (down 10 / up 5 dB, threshold at 2 ascending responses), 250 Hz–8 kHz, 1 kHz retest, random gaps between tones and false-positive counting. Tones are pulsed with 20 ms ramps.
- **Hearing aid:** **NAL-R** prescription per ear, 8-band minimum-phase EQ (biquads), WDRC compression, output limiter (MPO) and encoder volume control.
- **Low latency:** 32 kHz, 1 ms blocks, full-duplex I2S.
- **Web page** (configuration mode over a Wi-Fi access point): audiogram, gain curve, per-band fine tuning, calibration and test tones. The page is in Portuguese.

```mermaid
flowchart LR
    M[INMP441 mic] --> H[100 Hz high-pass]
    H --> W[WDRC]
    W --> G[Volume and gain per ear]
    G --> E[8-band EQ per ear]
    E --> L[MPO limiter]
    L --> D[PCM5102A DAC]
    D --> P[Headphones]
```

## Bill of materials

| Part | Notes |
|---|---|
| ESP32-WROOM-32 DevKit | The board's blue LED (D2) is the status light |
| INMP441 module | I2S microphone |
| PCM5102A module | I2S DAC with 3.5 mm output |
| Rotary encoder with push button | Any mechanical encoder should work |
| 18650 cell and TP4056 charger with protection | For battery use |
| 3.3 V low-dropout regulator | For battery use |
| Closed headphones | Open headphones cause feedback |

See [docs/hardware.md](docs/hardware.md) for wiring, power and calibration.

## Quick start

1. Wire the parts following [docs/hardware.md](docs/hardware.md).
2. Install [PlatformIO](https://platformio.org/) (VS Code extension or `pip install platformio`).
3. Build and flash:

```sh
pio run -t upload
```

4. Follow [First use](#first-use) below.

## Build, test and flash

```sh
pio run                       # build
pio run -t upload             # flash
pio device monitor            # serial log (115200)
pio test -e native            # tests on your computer (needs gcc on your PATH)
```

If flashing stops at `Connecting...` or fails with `Invalid head of packet`, put the board in download mode by hand: **hold BOOT, press and release EN, and release BOOT** after 2–3 s. Do not touch the board until it finishes.

Every 10 s, the serial log shows the DSP time per block (it should stay well below 1000 µs) and the level of each microphone slot.

## Usage

All control is through the **encoder** (turn and press), and feedback comes from the **board's blue LED (D2)**.

### First use

1. Connect the headphones (closed ones if possible) and set the **volume to minimum**.
2. Turn the device on. With no saved audiogram, **audiometry starts by itself** after 3 s (see below).
3. When it finishes, the device starts working with the gain prescribed for your thresholds.

### Controls

| Action | Function | LED |
|---|---|---|
| Turn | Volume (±2 dB per click) | Fast blink at the limit |
| Click | Mute on/off | Muted: 2 short blinks · Sound on: lit for 0.4 s |
| Double click | Battery level | 1 to 4 blinks (4 = full) |
| Hold 3 s | Start audiometry (during the test: **cancel**) | See below |
| Hold while powering on | Configuration mode (Wi-Fi and web page) | Lit while held, then 3 blinks |

With the battery divider fitted and `BATT_MONITOR_ENABLED` set to 1 (`src/board.h`), the LED blinks 3 times every minute below 3.40 V, and the device shuts itself down below 3.20 V to protect the battery. Without the divider, the double-click level is not reliable.

### Audiometry

Do this somewhere **quiet**, with the headphones well seated. It takes 10 to 15 minutes.

1. **Hold the button for 3 s.** The LED blinks twice and stays lit for the whole test.
2. At the start of each ear, the LED gives slow blinks: **1 = right ear** (tested first), **2 = left ear**.
3. You will hear short beeps (3 pulses) at random intervals. **Press the button whenever you hear one**, even if it is very faint. The LED turns off briefly to confirm the press.
4. Do not press without hearing a tone: presses during silence count as false positives and delay the next tone.
5. At the end:
   - **3 fast blinks** = test complete and saved; the device returns to hearing aid mode.
   - **1 long blink** = test canceled; nothing was saved and the previous audiogram stays in use.

To cancel partway through, hold the button for 3 s. If nobody responds to any tone, the result is discarded.

### Configuration mode (web page)

1. Turn the device off, then **hold the button and turn it on**. Release when the LED blinks 3 times.
2. On a phone or computer, connect to the **`HearingAid-XXXX`** network (password `aparelho123`).
3. Open **http://192.168.4.1**.

The page shows the audiogram and gain curve, and lets you fine-tune each band, choose the headphone type, calibrate and start audiometry (including **biological calibration**). See [Calibration](docs/hardware.md#calibration).

## Project structure

```
lib/hacore/     pure logic (no ESP-IDF), testable on a computer
  audiometry.*    Hughson-Westlake procedure
  nalr.*          NAL-R prescription
  eq_fit.*        8-band EQ and target fitting
  dynamics.*      WDRC and limiter
  ha_dsp.*        full hearing aid chain
  fitting.*       audiogram + settings -> DSP parameters
  ha_settings.*   settings and dB HL <-> dBFS calibration
  tone_gen.*      pulsed tone generator
src/            ESP-IDF firmware
  audio_io.*      full-duplex I2S
  audio_engine.*  real-time audio task (core 1)
  audiometry_runner.*  audiometry timing
  input.*         encoder (PCNT) and button
  webui.*, web/index.html
test/test_hacore  Unity tests
docs/           hardware guide and references
```

## Verification

1. **Passthrough and latency:** with an empty audiogram, the gain is flat (0 dB insertion). Record a click near the microphone and the headphone output at the same time (sound card or oscilloscope) and measure the delay. The target is under 5 ms.
2. **Response:** use "Tom de teste" (test tone) at each frequency and compare the output with the curve on the page.
3. **Safety:** with the headphones away from your ears, clap near the microphone at maximum volume and confirm that the output does not exceed the MPO.
4. **Audiometry:** a person with normal hearing should land between 0 and 20 dB HL (after biological calibration), and the 1 kHz retest should be within ±5 dB.

## References

The sources for the prescription, audiometry procedure and reference levels are in [docs/references.md](docs/references.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md). To report a security problem, see [SECURITY.md](SECURITY.md).

## License

Code is licensed under [Apache-2.0](LICENSE). Documentation, including this README and the files in `docs/`, is licensed under [CC BY 4.0](docs/LICENSE). See [THIRD_PARTY.md](THIRD_PARTY.md) for dependencies.
