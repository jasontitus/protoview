# Changelog

## v2.3 (2026-02-17)

- **Architectural refactor**: Move signal scanning and radio cycling out of
  the FuriOS timer callback into the main event loop. The timer now only
  sets lightweight flags; heavy work (scan_for_signal, tpms_extract_and_store,
  radio reconfiguration) runs in the main thread. This prevents the
  "ViewPort lockup" crash that occurred when 14 decoders blocked the GUI.

## v2.2 (2026-02-16)

- Move version display to main TPMS list header bar (was hidden behind
  "Modulation" label on settings screen).
- Add protocol validation test suite (272 checks against rtl_433
  reference data, 0 failures).
- Download and validate against rtl_433_tests captures for Schrader,
  Ford, Toyota PMV-107J, and Elantra2012.
- Include user-provided `tpms_realworld.jsonl` and `tpms_sample.jsonl`
  as additional test data sources.

## v2.1 (2026-02-16)

- Add version number display on settings screen.

## v2.0 (2026-02-16)

- **6 new TPMS decoders** ported from rtl_433 protocol documentation:
  - Toyota PMV-107J (US Highlander, Camry, Lexus) — FSK, differential
    Manchester, CRC-8 poly 0x13.
  - Hyundai Elantra 2012 / Honda Civic — FSK Manchester, CRC-8.
  - BMW Gen4/5 and Audi — FSK Manchester, CRC-8 poly 0x2F.
  - BMW Gen2/Gen3 — FSK differential Manchester, CRC-16.
  - Porsche Boxster/Cayman (Typ 987) — FSK differential Manchester,
    CRC-16.
  - Schrader SMD3MA4 (Subaru, Nissan, Infiniti) — OOK Manchester.
- **Rewrite GM decoder** to match actual rtl_433 GM-Aftermarket
  protocol (130-bit messages, 48-bit zero preamble, pressure * 2.75 kPa,
  temp - 60 C, byte-sum checksum).
- Add shared `diff_manchester_decode()` with correct 3-sample sliding
  window algorithm.
- Add `crc16()` for BMW Gen2/3 and Porsche decoders.
- Add Toyota-optimized CC1101 preset with 34.9 kHz FSK deviation.
- **14 total decoders** covering Ford, GM/Chevy/GMC, Stellantis
  (Chrysler/Dodge/Jeep/Ram), Toyota, BMW, Porsche, Hyundai, Honda,
  Subaru, Nissan, Renault, Citroen, and more.
- Rewrite README with supported vehicles table.
- Add CREDITS file with rtl_433 GPL-2.0 attribution.

## v1.1 (2026-02-15)

- Fix 315 MHz default frequency (was falling back to 433.92 MHz when
  315 MHz was not in Flipper's `setting_user` frequency list).
- Update for Flipper SDK v1.4.3 compatibility:
  - `cc1101.h` renamed to `cc1101_regs.h`.
  - Replace removed `furi_hal_subghz_load_preset()` with
    `furi_hal_subghz_load_custom_preset()` mapping.
- Add crash resilience: detected sensors saved to CSV log at
  `/ext/apps_data/tpms_reader/tpms_log.csv` on each detection.
- Fix duplicate `furi_hal_power_suppress_charge_enter()` call in
  `radio_begin()` that likely caused crashes during modulation cycling.
- Make frequency settings view robust when current frequency is not
  in the scrollable list.

## v1.0 (2026-02-15)

- Fork ProtoView into focused TPMS Reader for US 315 MHz signals.
- Strip non-TPMS decoders and views (Keeloq, Oregon2, PT2262, Chat,
  signal builder, direct sampling, generic decoder).
- Add TPMS-specific UI: sensor list view with tire ID, pressure (PSI),
  temperature (F), and receive count; detail view per sensor.
- Add auto-cycling through OOK and FSK modulation presets (~3 second
  rotation).
- Add sensor tracking with duplicate detection and update-in-place.
- Register only TPMS protocol decoders: Renault, Toyota (EU), Schrader
  GEN1, Schrader EG53MA4, Citroen, Ford, Hyundai/Kia, GM.

---

## ProtoView History (upstream, pre-fork)

### 2023

- Signal builder view for creating and editing messages from scratch.
- Keeloq (Microchip HCS200/300/301) encoder/decoder.
- Renault TPMS encoder.
- ProtoView Chat protocol for Flipper-to-Flipper messaging.
- General-purpose "unknown" signal decoder.
- Save/load signals as `.sub` files.
- Oscilloscope-alike decoded signal view with TX capability.
- Direct sampling mode reading CC1101 GDO0 pin.

### 2022

- Initial ProtoView release by Salvatore Sanfilippo.
- Raw signal detection and visualization.
- Manchester and differential Manchester line code decoding.
- Protocol decoders: Renault TPMS, Toyota TPMS, Schrader TPMS,
  Schrader EG53MA4, Citroen TPMS, Ford TPMS, Oregon2 thermometer,
  PT2262/SC5262 remotes.
- Custom CC1101 presets for TPMS signals.
- Frequency and modulation settings UI.
