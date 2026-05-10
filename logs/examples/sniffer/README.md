# Sniffer Example Captures

This folder contains sanitized example sniffer captures.

`good-stick-boot-0x53-baseline.txt` is a known-good module boot baseline. It was
captured with the `WROOM_LOW_RAM` profile.

`overflow=yes` means the 1024-event buffer filled. This is expected on the
low-RAM profile and does not indicate failure.

The capture is boot I2C / I2C-compatible sideband traffic, not full I3C decode.
Active SPD/HUB traffic appears at `0x53` in this captured HSA/strap state.
