# References

| Topic | Source | Used in |
|---|---|---|
| NAL-R prescription | Byrne, D. & Dillon, H. (1986). The National Acoustic Laboratories' (NAL) new procedure for selecting the gain and frequency response of a hearing aid. *Ear and Hearing*, 7(4), 257–265. | `lib/hacore/nalr.c` |
| Threshold search | Carhart, R. & Jerger, J. F. (1959). Preferred method for clinical determination of pure-tone thresholds. *Journal of Speech and Hearing Disorders*, 24, 330–345. | `lib/hacore/audiometry.c` |
| RETSPL, supra-aural (TDH-39) | ISO 389-1, *Acoustics: Reference zero for the calibration of audiometric equipment, Part 1*. | `lib/hacore/ha_settings.c` |
| RETSPL, insert (ER-3A) | ISO 389-2, *Acoustics: Reference zero for the calibration of audiometric equipment, Part 2*. | `lib/hacore/ha_settings.c` |

The firmware adapts these methods to low-cost hardware. It does not claim compliance with any of these standards. Its RETSPL values only apply to the reference headphones named above, so other headphones need [calibration](hardware.md#calibration).
