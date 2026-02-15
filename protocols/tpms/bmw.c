/* BMW Gen4/Gen5 and Audi TPMS decoder.
 * Multi-brand sensors: HUF/Beru, Continental, Schrader/Sensata, Audi.
 * FSK modulation, Manchester encoding, 315 MHz (US) / 433 MHz (EU).
 *
 * Preamble: 0xAA59 (16 bits: 1010101001011001)
 * Data: Manchester encoded, zero-bit inverted.
 *   BMW:  11 bytes (Brand, ID[4], Pressure, Temp, Flags[3], CRC)
 *   Audi:  8 bytes (Brand, ID[4], Pressure, Temp, CRC)
 *
 * Pressure kPa = raw * 2.45
 * Temperature C = raw - 52
 * CRC-8: poly 0x2F, init 0xAA
 *
 * Protocol documentation derived from rtl_433 project (GPL-2.0).
 * This is an independent implementation for the Flipper Zero platform. */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                   ProtoViewMsgInfo *info)
{
    if (numbits < 16 + 64 * 2) return false;

    /* Preamble: 0xAA59 = 1010101001011001 */
    uint32_t off = bitmap_seek_bits(bits, numbytes, 0, numbits,
                                    "1010101001011001");
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    info->start_off = off;
    off += 16;

    /* Manchester decode, zero-bit inverted: 10=0, 01=1. */
    uint8_t raw[11];
    memset(raw, 0, sizeof(raw));
    uint32_t decoded = convert_from_line_code(
        raw, sizeof(raw), bits, numbytes, off, "10", "01");

    /* Try BMW (11 bytes) first, then Audi (8 bytes). */
    bool is_bmw = (decoded >= 88);
    bool is_audi = (!is_bmw && decoded >= 64);

    if (!is_bmw && !is_audi) return false;

    uint8_t msg_len = is_bmw ? 11 : 8;
    uint8_t crc_len = msg_len - 1;

    /* CRC-8: poly 0x2F, init 0xAA. */
    if (crc8(raw, crc_len, 0xAA, 0x2F) != raw[crc_len]) return false;

    /* Extract fields. */
    uint8_t tire_id[4];
    tire_id[0] = raw[1];
    tire_id[1] = raw[2];
    tire_id[2] = raw[3];
    tire_id[3] = raw[4];

    float pressure_kpa = (float)raw[5] * 2.45f;
    int temp_c = (int)raw[6] - 52;

    info->pulses_count = (off + decoded * 2) - info->start_off;

    fieldset_add_bytes(info->fieldset, "Tire ID", tire_id, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1);
    fieldset_add_int(info->fieldset, "Temperature C", temp_c, 8);
    return true;
}

ProtoViewDecoder BMWTPMSDecoder = {
    .name = "BMW/Audi TPMS",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
