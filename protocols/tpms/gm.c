/* GM Aftermarket TPMS decoder.
 * Used in GM / Chevrolet / Buick / GMC / Cadillac with aftermarket sensors
 * (compatible with EL-50448 learning tool).
 * OOK modulation, Manchester zero-bit encoding, 315 MHz (US).
 *
 * Note: Many GM OEM sensors (Tahoe, Sierra, Yukon, etc.) use Schrader
 * EG53MA4 sensors which are handled by the Schrader EG53MA4 decoder.
 *
 * Preamble: 48 bits of zeros (6 bytes of 0x00).
 * Data: Manchester zero-bit encoded (10=0, 01=1).
 *   130 bits total after preamble:
 *   Bytes 0-5:  Preamble (all zeros, 48 bits)
 *   Bytes 6-7:  Flags (16 bits)
 *   Byte 8:     Device type
 *   Bytes 9-13: 40-bit Sensor ID
 *   Byte 14:    Pressure raw (kPa = raw * 2.75)
 *   Byte 15:    Temperature raw (C = raw - 60)
 *   Byte 16:    Checksum (sum of bytes 6-15 mod 256)
 *
 * Protocol documentation derived from rtl_433 project (GPL-2.0).
 * This is an independent implementation for the Flipper Zero platform. */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                   ProtoViewMsgInfo *info)
{
    /* Need at least 48-bit preamble + 82 data bits * 2 (Manchester). */
    if (numbits < 48 + 82 * 2) return false;

    /* Preamble: 48 bits of zeros = 48 Manchester symbols "10" = 96 raw bits
     * of alternating 1010...  We search for a chunk of this pattern. */
    const char *preamble = "101010101010101010101010"
                           "101010101010101010101010";
    uint32_t off = bitmap_seek_bits(bits, numbytes, 0, numbits, preamble);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    info->start_off = off;

    /* Manchester zero-bit decode from start of preamble: 10=0, 01=1. */
    uint8_t raw[17];
    memset(raw, 0, sizeof(raw));
    uint32_t decoded = convert_from_line_code(
        raw, sizeof(raw), bits, numbytes, off, "10", "01");

    if (decoded < 130) return false;

    /* First 6 bytes (48 bits) should be all zeros (preamble). */
    for (int i = 0; i < 6; i++) {
        if (raw[i] != 0x00) return false;
    }

    /* Checksum: sum of bytes 6-15 mod 256. */
    uint8_t sum = sum_bytes(raw + 6, 10, 0);
    if (sum != raw[16]) return false;

    /* Extract fields. */
    uint8_t tire_id[5];
    tire_id[0] = raw[9];
    tire_id[1] = raw[10];
    tire_id[2] = raw[11];
    tire_id[3] = raw[12];
    tire_id[4] = raw[13];

    float pressure_kpa = (float)raw[14] * 2.75f;
    int temp_c = (int)raw[15] - 60;

    /* Basic sanity. */
    if (pressure_kpa > 1000.0f || pressure_kpa < 0.0f) return false;

    info->pulses_count = (off + decoded * 2) - info->start_off;

    fieldset_add_bytes(info->fieldset, "Tire ID", tire_id, 5 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1);
    fieldset_add_int(info->fieldset, "Temperature C", temp_c, 8);
    return true;
}

ProtoViewDecoder GMTPMSDecoder = {
    .name = "GM TPMS",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
