/* Pacific PMV-107J TPMS decoder.
 * Used in Toyota Highlander (2015+), Camry, Corolla, Lexus, etc.
 * US market: 315 MHz. Other markets: 433.92 MHz.
 *
 * FSK modulation, differential Manchester encoding.
 * ~10 kBaud data rate, ~35 kHz deviation.
 *
 * Preamble: 11111 10 (5 ones + reference clock)
 * Data: 66 bits differential Manchester → 9 bytes after realignment.
 *
 * Byte layout after realignment (first 2 decoded bits shifted):
 *   b[0]: 000000II  (MSB 2 bits of ID)
 *   b[1]: IIIIIIII
 *   b[2]: IIIIIIII
 *   b[3]: IIIIIIII
 *   b[4]: IISSSSSS  (LSB 2 bits of ID + 6 status bits)
 *   b[5]: PPPPPPPP  (pressure raw)
 *   b[6]: NNNNNNNN  (inverted pressure, must XOR to 0xFF with b[5])
 *   b[7]: TTTTTTTT  (temperature raw)
 *   b[8]: CCCCCCCC  (CRC-8, poly 0x13, init 0x00)
 *
 * Pressure kPa = (b[5] - 40) * 2.48
 * Temperature C = b[7] - 40
 *
 * Reference: rtl_433 tpms_pmv107j.c decoder. */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                   ProtoViewMsgInfo *info)
{
    /* Need preamble (6 bits) + at least 66*2 raw bits for diff Manchester. */
    if (numbits < 6 + 66 * 2) return false;

    /* Search for preamble: 111110 (five ones + first half of reference). */
    uint32_t off = bitmap_seek_bits(bits, numbytes, 0, numbits, "111110");
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    FURI_LOG_D(TAG, "PMV-107J preamble found at %lu", off);
    info->start_off = off;
    off += 6; /* Skip preamble, start at second half of reference clock. */

    /* Differential Manchester decode. We want 66 data bits. */
    uint8_t decoded_buf[10];
    memset(decoded_buf, 0, sizeof(decoded_buf));
    uint32_t decoded = diff_manchester_decode(
        decoded_buf, sizeof(decoded_buf), bits, numbytes, off, 70);

    FURI_LOG_D(TAG, "PMV-107J diff manchester decoded %lu bits", decoded);
    if (decoded < 66) return false;

    /* Realign: first 2 decoded bits go to b[0], next 64 bits to b[1..8].
     * This matches the rtl_433 realignment. */
    uint8_t b[9];
    memset(b, 0, sizeof(b));
    b[0] = (bitmap_get(decoded_buf, sizeof(decoded_buf), 0) ? 0x02 : 0) |
           (bitmap_get(decoded_buf, sizeof(decoded_buf), 1) ? 0x01 : 0);
    bitmap_copy(b + 1, 8, 0, decoded_buf, sizeof(decoded_buf), 2, 64);

    /* CRC-8 check: poly 0x13, init 0x00 over bytes 0-7, must equal byte 8. */
    uint8_t crc = crc8(b, 8, 0x00, 0x13);
    if (crc != b[8]) {
        FURI_LOG_D(TAG, "PMV-107J CRC mismatch: calc=%02X got=%02X", crc, b[8]);
        return false;
    }

    /* Pressure integrity: b[5] and b[6] XOR must be 0xFF. */
    if ((b[5] ^ b[6]) != 0xFF) {
        FURI_LOG_D(TAG, "PMV-107J pressure check failed: %02X ^ %02X != FF",
                   b[5], b[6]);
        return false;
    }

    /* Extract fields. */
    uint8_t tire_id[4];
    tire_id[0] = (b[0] << 6) | (b[1] >> 2);
    tire_id[1] = (b[1] << 6) | (b[2] >> 2);
    tire_id[2] = (b[2] << 6) | (b[3] >> 2);
    tire_id[3] = (b[3] << 6) | (b[4] >> 2);
    /* 28-bit ID is in tire_id[0..3] with lower 4 bits of tire_id[3] unused.
     * Actually the ID is: b[0]<<26 | b[1]<<18 | b[2]<<10 | b[3]<<2 | b[4]>>6
     * For our fieldset, store the 4 raw bytes. */

    float pressure_kpa = (b[5] - 40.0f) * 2.48f;
    int temp_c = (int)b[7] - 40;

    info->pulses_count = decoded * 2 + 6; /* Approximate raw pulse span. */

    fieldset_add_bytes(info->fieldset, "Tire ID", tire_id, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 2);
    fieldset_add_int(info->fieldset, "Temperature C", temp_c, 8);
    return true;
}

ProtoViewDecoder PMV107JTPMSDecoder = {
    .name = "Toyota PMV-107J",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
