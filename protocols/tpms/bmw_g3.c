/* BMW Gen2/Gen3 TPMS decoder.
 * FSK modulation, differential Manchester encoding, 315 MHz (US) / 433 MHz (EU).
 *
 * Preamble: 0xCCCD (16 bits: 1100110011001101)
 * Data: differential Manchester encoded.
 *   Gen3: 11 bytes (ID[4], Pressure, Temp, Flags[3], CRC16[2])
 *   Gen2: 10 bytes (ID[4], Pressure, Temp, Flags[3], CRC16[1]?)
 *
 * Pressure kPa = (raw - 43) * 2.5
 * Temperature C = raw - 40
 * CRC-16: poly 0x1021, init 0x0000
 *
 * Protocol documentation derived from rtl_433 project (GPL-2.0).
 * This is an independent implementation for the Flipper Zero platform. */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                   ProtoViewMsgInfo *info)
{
    if (numbits < 16 + 88 * 2) return false;

    /* Preamble: 0xCCCD = 1100110011001101 */
    uint32_t off = bitmap_seek_bits(bits, numbytes, 0, numbits,
                                    "1100110011001101");
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    info->start_off = off;
    off += 16;

    /* Differential Manchester decode. */
    uint8_t raw[11];
    memset(raw, 0, sizeof(raw));
    uint32_t decoded = diff_manchester_decode(
        raw, sizeof(raw), bits, numbytes, off, 90);

    /* Gen3 needs >= 88 bits (11 bytes), Gen2 >= 80 bits (10 bytes). */
    bool is_gen3 = (decoded >= 88);
    bool is_gen2 = (!is_gen3 && decoded >= 80);
    if (!is_gen3 && !is_gen2) return false;

    uint8_t msg_len = is_gen3 ? 11 : 10;

    /* CRC-16: poly 0x1021, init 0x0000 over all bytes. */
    if (crc16(raw, msg_len, 0x0000, 0x1021) != 0) return false;

    /* Extract fields. */
    uint8_t tire_id[4];
    tire_id[0] = raw[0];
    tire_id[1] = raw[1];
    tire_id[2] = raw[2];
    tire_id[3] = raw[3];

    float pressure_kpa = ((float)raw[4] - 43.0f) * 2.5f;
    int temp_c = (int)raw[5] - 40;

    info->pulses_count = (off + decoded * 2) - info->start_off;

    fieldset_add_bytes(info->fieldset, "Tire ID", tire_id, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1);
    fieldset_add_int(info->fieldset, "Temperature C", temp_c, 8);
    return true;
}

ProtoViewDecoder BMWGen3TPMSDecoder = {
    .name = "BMW Gen2/3 TPMS",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
