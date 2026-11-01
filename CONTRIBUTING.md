# Contributing

Thanks for helping! Issues and pull requests are welcome in English or Portuguese.

Read [SAFETY.md](SAFETY.md) first. Never test changes to gain, tones or the limiter with headphones on your ears until you have measured the output.

## Setup

You need [PlatformIO](https://platformio.org/) and, for the PC tests, `gcc` on your `PATH` (on Windows, use MinGW-w64 or MSYS2).

```sh
pio test -e native   # unit tests on your computer
pio run              # builds the firmware
```

If `pio` is not on your `PATH`, use `python -m platformio` instead.

## Project layout

- `lib/hacore/` holds the pure logic, with no ESP-IDF dependency. Put anything testable here.
- `src/` holds the ESP-IDF firmware. The audio task is real time, so it must not block or allocate memory.
- `test/test_hacore/` holds the Unity tests.

## Pull requests

- Keep each pull request focused on one change.
- Follow the style of the surrounding code. Existing code comments are in Portuguese; new ones can be in Portuguese or English.
- Update both language versions of the documentation when behavior changes. If you only write one language, say so in the pull request and someone can help with the other.
- CI must pass.

## Changes that affect output level

Any change to gain, EQ, compression, limiting, calibration, tone generation or audiometry **must** include a test in `test/test_hacore/`. Describe in the pull request how you checked the real output.

## Hardware changes

If you change a pin in `src/board.h`, update `docs/hardware.md` and `docs/pt-BR/hardware.md` in the same pull request.

## License

By contributing, you agree that your code is licensed under Apache-2.0 and your documentation under CC BY 4.0.
