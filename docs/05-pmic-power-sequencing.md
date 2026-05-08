# PMIC Power Sequencing Notes

The currently referenced PMIC family is Richtek RTQ5119A-style DDR5 VR-on-DIMM PMIC behavior.

## Rails and management power

Key PMIC concepts:

| Concept | Meaning |
|---|---|
| VIN_BULK | main input supply for output regulators |
| VIN_MGMT | management supply used for PMIC NVM/register access and LDO support |
| VLDO_1.8V | supplies SPD5 hub / TS-related 1.8 V domain |
| VLDO_1.0V | supports 1.0 V I3C push-pull/internal use |
| CAMP | combined control/monitor/status/write-protect related open-drain line |
| GSI_n | general status interrupt output |

## VR enable rule

Do not issue PMIC VR Enable immediately after applying power. The PMIC documentation calls out minimum stable times for both management and bulk supplies before VR enable.

For the referenced RTQ5119A behavior:

- VIN_MGMT must be valid before reliable management access.
- The management interface becomes available after the management-ready delay.
- VR Enable is done by setting PMIC register `0x32[7] = 1` or equivalent command.
- Status registers should be queried before enabling VR.

## CAMP behavior in plain English

CAMP is not just “power good.” It has multiple roles:

1. write-protect/configuration state
2. fail/status signaling
3. PMIC power-good style output after VR enable

That makes CAMP a dangerous line to oversimplify in firmware names. Use explicit names like `camp_status`, `camp_fault`, or `camp_write_protect_state` in future code/docs where possible.
