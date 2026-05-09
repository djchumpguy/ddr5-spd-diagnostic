# Source Index

This repo should track source material without necessarily committing copyrighted PDFs.

## Local/reference documents used while seeding this repo

| Source | What it contributes |
|---|---|
| ABLIC S-34HTS08AB SPD5 Hub datasheet | SPD hub architecture, HSA/HID behavior, I2C/I3C modes, MR11/MR12/MR13 register behavior |
| Netac/UniIC DDR5 UDIMM datasheets | 288-pin UDIMM pinout, VIN_BULK/HSCL/HSDA/HSA/PWR_EN/PWR_GOOD pin references, SPD block layout |
| Richtek RTQ5119A PMIC datasheet | PMIC power sequencing, VR Enable, CAMP/GSI_n behavior, PMIC register/status behavior |
| Bus Pirate DDR5 SPD docs | External practical reference for DDR5 SPD/I2C hub experimentation |
| Project wiring notes | Actual ESP32 GPIO mapping, VIN_BULK power options, manual HSA workflow, PWR_GOOD readiness behavior, PCA9306/direct-I2C wiring notes |
| Project suspicious register notes | Good/bad stick MR11/MR12/MR13 findings and hypotheses |

## Suggested `sources/` policy

- Store public URLs and document titles here.
- Do not commit PDFs unless redistribution is allowed.
- If PDFs are needed for private work, place them in `sources/private/`; `.gitignore` excludes that directory.
