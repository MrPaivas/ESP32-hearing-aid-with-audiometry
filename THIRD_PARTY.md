# Third-party software

No third-party source code is copied into this repository. PlatformIO downloads these dependencies at build time.

| Component | Used for | License |
|---|---|---|
| [ESP-IDF](https://github.com/espressif/esp-idf) | Firmware framework | Apache-2.0 |
| [Unity](https://github.com/ThrowTheSwitch/Unity) | Unit tests only | MIT |

ESP-IDF bundles other components with their own licenses, which apply to compiled firmware binaries. If you distribute a binary, check the licenses of the components it includes.

The NAL-R formula and the RETSPL reference values come from published sources, listed in [docs/references.md](docs/references.md). This repository does not reproduce the text of any ISO standard.
