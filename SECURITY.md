# Security policy

## Reporting a vulnerability

Please do not open a public issue. Use [GitHub private vulnerability reporting](https://github.com/MrPaivas/ESP32-hearing-aid-with-audiometry/security/advisories/new) instead.

This is a volunteer project, so response times are not guaranteed. Reports that can cause unsafe sound levels are handled first.

## Supported versions

Only the latest commit on `main` is supported.

## Known limitations

- **Shared default Wi-Fi password.** `WIFI_AP_PASSWORD` in `src/board.h` is the same for every device and is public in this repository. Change it before building if other people may be within Wi-Fi range.
- **No authentication on the web API.** While configuration mode is active, any device connected to the access point can use `/api/settings` and `/api/tone`, which can change the output limit and play tones.

Configuration mode only starts if the button is held during power-on. See also [SAFETY.md](SAFETY.md).
