# Repository Discovery Notes

[Back to README](../README.md)

This page tracks public presentation suggestions for the GitHub repository. It is not
firmware documentation and does not change project behavior.

## Recommended Repository Description

```text
ESP32 DDR5 SPD/PMIC diagnostic tool and passive DDR5 boot sniffer for SPD hub, PMIC, and DDR5 management-plane debugging.
```

## Recommended GitHub Topics

- `ddr5`
- `spd`
- `spd5118`
- `spd5119`
- `pmic`
- `esp32`
- `i2c`
- `platformio`
- `ram-repair`
- `hardware-debugging`
- `memory-diagnostics`
- `firmware`
- `embedded`

These topics help GitHub classify the repo. They are only one part of discovery:
README quality, release assets, examples, links, issue activity, and continued project
activity also matter.

## Suggested GitHub CLI Command

Run this manually after confirming `gh auth status` is healthy:

```bash
gh repo edit djchumpguy/ddr5-spd-diagnostic \
  --description "ESP32 DDR5 SPD/PMIC diagnostic tool and passive DDR5 boot sniffer for SPD hub, PMIC, and DDR5 management-plane debugging." \
  --add-topic ddr5 \
  --add-topic spd \
  --add-topic spd5118 \
  --add-topic spd5119 \
  --add-topic pmic \
  --add-topic esp32 \
  --add-topic i2c \
  --add-topic platformio \
  --add-topic ram-repair \
  --add-topic hardware-debugging \
  --add-topic memory-diagnostics \
  --add-topic firmware \
  --add-topic embedded
```

Do not use this command to change repository visibility, default branch, or enabled
features.

## Social Preview And About Link

Suggested social preview image:

- A clean Web UI screenshot from `docs/images/ui/`, or
- the simple active SPD Tool wiring diagram from `docs/hardware/spd-tool-wiring.md`.

Suggested repository About link:

- latest release page, or
- the README if the release page is not the intended entry point.

The social preview and About link normally need to be set manually in GitHub repository
settings unless a future API-based maintenance task is explicitly approved.
