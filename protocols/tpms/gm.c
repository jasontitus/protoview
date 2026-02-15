/* GM / Chevrolet / Buick / GMC / Cadillac TPMS.
 * Common on US-market GM vehicles at 315 MHz OOK.
 *
 * Many GM vehicles use sensors based on the Pacific Industries / TRW
 * design. This is a distinct format from the Schrader sensors also
 * used by some GM models (those are handled by the Schrader decoder).
 *
 * Modulation: OOK, ~120us short pulse.
 * Preamble: alternating 010101... (at least 8 symbols)
 * Sync: specific pattern marking the start of data.
 * Encoding: Manchester (01 = 0, 10 = 1)
 * Data: 9 bytes total.
 *
 * Byte layout:
 *   Byte 0:    Status / message type
 *   Bytes 1-4: 32-bit Sensor ID
 *   Byte 5:    Pressure raw (pressure_kPa = raw * 2.5 + offset)
 *   Byte 6:    Temperature raw (temp_C = raw - 50)
 *   Byte 7:    Status flags
 *   Byte 8:    CRC-8 (poly 0x07, init 0x00, bytes 0-7)
 */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    /* GM TPMS uses a distinct sync pattern with a longer initial pulse
     * followed by alternating preamble bits and a specific sync word. */
    const char *sync_pattern = "0101010101010101" "0110";
    uint8_t sync_len = 16 + 4;
    if (numbits - sync_len < 9 * 8 * 2) return false;

    uint64_t off = bitmap_seek_bits(bits, numbytes, 0, numbits, sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;
    FURI_LOG_E(TAG, "GM TPMS preamble+sync found");

    info->start_off = off;
    off += sync_len;

    uint8_t raw[9];
    uint32_t decoded =
        convert_from_line_code(raw, sizeof(raw), bits, numbytes, off,
            "01", "10"); /* Manchester code. */
    FURI_LOG_E(TAG, "GM TPMS decoded bits: %lu", decoded);

    if (decoded < 9 * 8) return false;

    /* CRC-8 check: poly 0x07, init 0x00, over bytes 0-7. */
    uint8_t crc = crc8(raw, 8, 0x00, 0x07);
    if (crc != raw[8]) {
        /* Try alternative: sum of bytes mod 256. */
        uint8_t sum = sum_bytes(raw, 8, 0);
        if (sum != raw[8]) {
            FURI_LOG_E(TAG, "GM TPMS CRC mismatch");
            return false;
        }
    }

    info->pulses_count = (off + 9 * 8 * 2) - info->start_off;

    uint8_t id[4];
    id[0] = raw[1];
    id[1] = raw[2];
    id[2] = raw[3];
    id[3] = raw[4];

    float kpa = (float)raw[5] * 2.5f;
    int temp = raw[6] - 50;
    int flags = raw[7];

    fieldset_add_bytes(info->fieldset, "Tire ID", id, 4 * 2);
    fieldset_add_float(info->fieldset, "Pressure kpa", kpa, 2);
    fieldset_add_int(info->fieldset, "Temperature C", temp, 8);
    fieldset_add_hex(info->fieldset, "Flags", flags, 8);
    return true;
}

ProtoViewDecoder GMTPMSDecoder = {
    .name = "GM TPMS",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
