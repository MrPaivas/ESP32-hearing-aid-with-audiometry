# Safety

[Português](SAFETY.pt-BR.md)

**This is an experimental project. It is not a medical device.** Read this page before building or wearing it. The software comes with no warranty; see [LICENSE](LICENSE).

## Not a medical device

- It has not been clinically validated, or evaluated or approved by any regulator.
- It does not replace a hearing test by an audiologist or a prescribed hearing aid.
- Its audiometry results are not a diagnosis. If you notice hearing loss, tinnitus, ear pain or dizziness, see a health professional.

## Risk of hearing damage

This device plays amplified sound directly into the ears. Loud sound can cause **permanent** hearing damage.

- Always set the volume to minimum before putting on the headphones, then raise it slowly.
- Keep the output limit (MPO) conservative. Do not raise it without a calibrated measurement.
- Remove the headphones immediately if the sound is painful, uncomfortable or unexpectedly loud.
- Check the output limiter after any firmware or settings change, with the headphones away from your ears. See "Verification" in the [README](README.md#verification).
- A firmware bug, a wiring fault or a different headphone can produce much louder sound than expected.

## SPL values depend on calibration

The firmware accepts `mpo_spl` values up to 110 dB SPL. **The dB SPL and dB HL values it shows only match reality after calibration with the exact headphones in use.** Without calibration, the real output may be much higher than the value shown. Changing headphones invalidates the calibration. See [Calibration](docs/hardware.md#calibration).

## Wireless settings access

Configuration mode opens a Wi-Fi network whose default password is public in this repository. Anyone in range who connects can play test tones and change settings, including the output limit, while that mode is active. Leave configuration mode when you finish, and do not wear the headphones while it is active around other people. See [SECURITY.md](SECURITY.md).

## Batteries

The design uses a lithium-ion 18650 cell, which can catch fire if short-circuited, damaged or over-discharged. Use a charger module with protection circuitry (TP4056 with DW01 or similar), and do not leave the device charging unattended. The firmware's low-battery protection is **disabled by default** and needs the voltage divider described in [docs/hardware.md](docs/hardware.md).

## Distribution and regulation

Hearing aids and audiometers are regulated products in many countries, for example by ANVISA in Brazil and the FDA in the United States. If you sell, donate or otherwise distribute assembled devices or firmware binaries, you are responsible for complying with local rules. The license for this project does not grant any regulatory approval.
