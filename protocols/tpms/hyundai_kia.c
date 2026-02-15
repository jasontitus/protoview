/* Hyundai / Kia TPMS (Continental/VDO sensors).
 * Common on US-market Hyundai and Kia vehicles at 315 MHz.
 * Also found at 433.92 MHz on European models.
 *
 * Modulation: FSK, ~52us short pulse.
 * Preamble: alternating 010101...
 * Sync: 0110
 * Encoding: Manchester (01 = 0, 10 = 1)
 * Data: 10 bytes total.
 *
 * Byte layout:
 *   Byte 0:    Message type / status flags
 *   Bytes 1-4: 32-bit Sensor ID
 *   Byte 5:    Battery / status
 *   Byte 6:    Pressure raw (pressure_kPa = raw * 2.5)
 *   Byte 7:    Temperature raw (temp_C = raw - 50)
 *   Byte 8:    Spare / flags
 *   Byte 9:    CRC-8 (XOR of bytes 0-8)
 */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    const char *sync_pattern = "010101010101" "0110";
    uint8_t sync_len = 12 + 4;
    if (numbits - sync_len < 10 * 8 * 2) return false;

    uint64_t off = bitmap_seek_bits(bits, numbytes, 0, numbits, sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;
    FURI_LOG_E(TAG, "Hyundai/Kia TPMS preamble+sync found");

    info->start_off = off;
    off += sync_len;

    uint8_t raw[10];
    uint32_t decoded =
        convert_from_line_code(raw, sizeof(raw), bits, numbytes, off,
            "01", "10"); /* Manchester code. */
    FURI_LOG_E(TAG, "Hyundai/Kia TPMS decoded bits: %lu", decoded);

    if (decoded < 10 * 8) return false;

    /* CRC check: XOR of bytes 0 through 8 should equal byte 9. */
    uint8_t crc = xor_bytes(raw, 9, 0);
    if (crc != raw[9]) {
        FURI_LOG_E(TAG, "Hyundai/Kia TPMS CRC mismatch");
        return false;
    }

    info->pulses_count = (off + 10 * 8 * 2) - info->start_off;

    uint8_t id[4];
    id[0] = raw[1];
    id[1] = raw[2];
    id[2] = raw[3];
    id[3] = raw[4];

    float kpa = (float)raw[6] * 2.5f;
    int temp = raw[7] - 50;
    int battery = raw[5] & 0x7f;
    int flags = raw[0];

    fieldset_add_bytes(info->fieldset, "Tire ID", id, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", kpa, 2);
    fieldset_add_int(info->fieldset, "Temperature C", temp, 8);
    fieldset_add_uint(info->fieldset, "Battery", battery, 7);
    fieldset_add_hex(info->fieldset, "Flags", flags, 8);
    return true;
}

ProtoViewDecoder HyundaiKiaTPMSDecoder = {
    .name = "Hyundai/Kia TPMS",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
