/* Porsche Boxster/Cayman (Typ 987) TPMS decoder.
 * FSK modulation, differential Manchester encoding, 315 MHz (US) / 433 MHz (EU).
 *
 * Preamble: alternating 1100 pairs ending in 1010 (~30 bits).
 * Data: 80 bits (10 bytes) differential Manchester encoded.
 *
 *   Bytes 0-3: 32-bit sensor ID
 *   Byte 4:    Pressure raw (kPa = raw * 5 / 2 - 100)
 *   Byte 5:    Temperature raw (C = raw - 40)
 *   Bytes 6-7: Status flags
 *   Bytes 8-9: CRC-16, poly 0x1021, init 0xFFFF
 *
 * Protocol documentation derived from rtl_433 project (GPL-2.0).
 * This is an independent implementation for the Flipper Zero platform. */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                   ProtoViewMsgInfo *info)
{
    if (numbits < 20 + 80 * 2) return false;

    /* Search for end of preamble: ...110011001010.
     * The preamble is alternating 1100 pairs that end with 1010. */
    uint32_t off = bitmap_seek_bits(bits, numbytes, 0, numbits,
                                    "110011001010");
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    info->start_off = off;
    off += 12; /* Skip the matched preamble tail. */

    /* Differential Manchester decode. */
    uint8_t raw[10];
    memset(raw, 0, sizeof(raw));
    uint32_t decoded = diff_manchester_decode(
        raw, sizeof(raw), bits, numbytes, off, 82);

    if (decoded < 80) return false;

    /* CRC-16: poly 0x1021, init 0xFFFF over all 10 bytes should give 0. */
    if (crc16(raw, 10, 0xFFFF, 0x1021) != 0) return false;

    /* Extract fields. */
    uint8_t tire_id[4];
    tire_id[0] = raw[0];
    tire_id[1] = raw[1];
    tire_id[2] = raw[2];
    tire_id[3] = raw[3];

    float pressure_kpa = (float)raw[4] * 2.5f - 100.0f;
    int temp_c = (int)raw[5] - 40;

    info->pulses_count = (off + decoded * 2) - info->start_off;

    fieldset_add_bytes(info->fieldset, "Tire ID", tire_id, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1);
    fieldset_add_int(info->fieldset, "Temperature C", temp_c, 8);
    return true;
}

ProtoViewDecoder PorscheTPMSDecoder = {
    .name = "Porsche TPMS",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
