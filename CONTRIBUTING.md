# Contributing / Updating Notes

## Style

- Keep notes practical.
- Prefer tables for pinouts/registers.
- Include exact mode/address/register/value where possible.
- Mark hypotheses clearly.
- Mark resolved findings clearly.

## Commit hygiene

Good commit examples:

```text
docs: add HSA cold-cycle warning
hardware: update GPIO map for dimm power switch
investigation: record MR12/MR13 good-vs-bad comparison
```

Avoid committing:

- raw binary dumps without context
- bulky datasheet PDFs unless licensed for redistribution
- logs with unrelated personal/system data
- old hypotheses as if they are current facts
