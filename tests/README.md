# TPMS Protocol Validation Tests

Validates decoder protocol math (CRC algorithms, pressure/temperature
conversion formulas, field extraction) against real-world data.

## Running

```bash
python3 tests/validate_protocols.py
```

## Test Data Sources

### User JSONL files (in project root)
- `tpms_realworld.jsonl` — real rtl_433 decoded output from various sensors
- `tpms_sample.jsonl` — sample decoded records

### rtl_433 reference captures (optional, improves coverage)

To download rtl_433 test captures (stored OUTSIDE the source tree
to avoid interfering with the Flipper build system):

```bash
mkdir -p ../rtl_433_tests && cd ../rtl_433_tests
git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/merbanan/rtl_433_tests.git .
git sparse-checkout set \
    tests/Schrader \
    tests/Schrader_EG53MA4_TPMS \
    tests/Schrader_TPMS_SMD3MA4 \
    tests/Ford_TPMS \
    tests/Toyota_TPMS \
    tests/Elantra2012TPMS
```

This downloads .cu8 IQ captures and reference .json files for 6
TPMS protocols at 315 MHz.

## What Gets Validated

- CRC-8/CRC-16 implementations against standard test vectors
- Pressure conversion formulas for all 14 protocols
- Temperature conversion formulas
- Protocol field values against rtl_433 decoded reference output
- 272+ checks across 112 reference files
