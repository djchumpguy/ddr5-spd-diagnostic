# Troubleshooting

[Back to README](../README.md) | [Safety](safety.md) | [Quick start](quick-start.md) | [Command reference](spd-tool/command-reference.md)

## No Devices Found

- Recheck grounds and rails.
- Confirm SDA/SCL are not swapped.
- Confirm the ESP32 and DIMM supply share ground.
- Confirm PWR_EN and PWR_GOOD have their 10 kOhm pull-ups.
- If the direct-read harness is flaky, consider optional external SDA/SCL pull-ups or level shifting as troubleshooting.
- Confirm VIN_BULK is actually present if your setup requires it.
- Cold-cycle VIN_BULK after HSA strap changes.

## PWR_GOOD Looks Wrong

PWR_GOOD is only meaningful according to your hardware config. A low or floating signal can mean wiring/readiness trouble; it is not automatic proof of bad SPD, PMIC, or DRAM.

## Address Changes Between 0x50, 0x53, And 0x57

Record the HSA physical strap, whether VIN_BULK was cold-cycled, and the harness wiring. In this project, `0x50` direct-ground/offline-style and `0x57` floating/high behavior were harness-dependent observations.

## MR11, MR12, And MR13 Confusion

MR11 is a page/pointer register and can change depending on access path. MR12/MR13 protected/unprotected differences are historical diagnostic context unless new captures prove they matter in your current case.

## Clean SPD But Still No Boot

That can happen. SPD payload verify, PMIC access, hub access, CRC/checksum, and read stability are management-plane evidence only. The included bad-stick case still does not boot; current evidence points toward DRAM-side failure from boot-time/sniffer behavior.
