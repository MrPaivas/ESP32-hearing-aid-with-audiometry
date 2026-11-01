# Changelog

This project follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/).

## [0.1.0] - 2026-11-01

First public version.

### Added

- Pure-tone audiometry per ear, using a modified Hughson-Westlake procedure.
- NAL-R fitting per ear, with an 8-band minimum-phase EQ, WDRC compression and an output limiter.
- Full-duplex I2S audio at 32 kHz with 1 ms blocks.
- Web configuration page over a Wi-Fi access point.
- Unit tests for the pure logic in `lib/hacore`.
