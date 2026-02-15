/* Hyundai Elantra 2012 / Honda Civic TPMS (TRW sensor, FCC ID GQ4-44T).
 * FSK modulation, Manchester encoding, 315 MHz (US) / 433 MHz (EU).
 *
 * Preamble: 0x7155 (16 bits: 0111000101010101)
 * Data: 64 bits Manchester encoded → 8 bytes.
 *
 * Byte layout: PP TT II II II II FF CC
 *   PP: pressure raw (kPa = raw + 60)
 *   TT: temperature raw (C = raw - 50)
 *   II: 32-bit sensor ID
 *   FF: flags (storage, battery, trigger)
 *   CC: CRC-8, poly 0x07, init 0x00
 *
 * Protocol documentation derived from rtl_433 project (GPL-2.0).
 * This is an independent implementation for the Flipper Zero platform. */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                   ProtoViewMsgInfo *info)
{
    if (numbits < 16 + 64 * 2) return false;

    /* Preamble: 0x7155 = 0111000101010101 */
    uint32_t off = bitmap_seek_bits(bits, numbytes, 0, numbits,
                                    "0111000101010101");
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    info->start_off = off;
    off += 16;

    /* Manchester decode: 01=0, 10=1. */
    uint8_t raw[8];
    uint32_t decoded = convert_from_line_code(
        raw, sizeof(raw), bits, numbytes, off, "01", "10");
    if (decoded < 64) return false;

    /* CRC-8 check: poly 0x07, init 0x00 over first 7 bytes. */
    if (crc8(raw, 7, 0x00, 0x07) != raw[7]) return false;

    /* Extract fields. */
    float pressure_kpa = (float)raw[0] + 60.0f;
    int temp_c = (int)raw[1] - 50;

    uint8_t tire_id[4];
    tire_id[0] = raw[2];
    tire_id[1] = raw[3];
    tire_id[2] = raw[4];
    tire_id[3] = raw[5];

    info->pulses_count = (off + 64 * 2) - info->start_off;

    fieldset_add_bytes(info->fieldset, "Tire ID", tire_id, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1);
    fieldset_add_int(info->fieldset, "Temperature C", temp_c, 8);
    return true;
}

ProtoViewDecoder Elantra2012TPMSDecoder = {
    .name = "Elantra2012 TPMS",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
