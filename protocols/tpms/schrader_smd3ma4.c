/* Schrader SMD3MA4 TPMS decoder.
 * Used in Subaru, Infiniti, Nissan, some Renault.
 * OOK modulation, Manchester encoding, 315 MHz (US) / 433 MHz (EU).
 *
 * Preamble: long sequence of alternating bits ending with 1110.
 * Data: Manchester encoded.
 *   3 bits:  flags
 *   24 bits: sensor ID
 *   10 bits: pressure raw (PSI = raw * 0.2)
 *   2 bits:  parity/check
 *
 * No temperature data in this protocol.
 *
 * Protocol documentation derived from rtl_433 project (GPL-2.0).
 * This is an independent implementation for the Flipper Zero platform. */

#include "../../app.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits,
                   ProtoViewMsgInfo *info)
{
    if (numbits < 12 + 39 * 2) return false;

    /* Preamble ends with ...01010101 1110.
     * Search for the tail of the preamble. */
    uint32_t off = bitmap_seek_bits(bits, numbytes, 0, numbits,
                                    "010101011110");
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    info->start_off = off;
    off += 12;

    /* Manchester decode: 01=0, 10=1. We need 39 bits. */
    uint8_t raw[5];
    memset(raw, 0, sizeof(raw));
    uint32_t decoded = convert_from_line_code(
        raw, sizeof(raw), bits, numbytes, off, "01", "10");

    if (decoded < 39) return false;

    /* Reject all-zero data. */
    if (raw[0] == 0 && raw[1] == 0 && raw[2] == 0 && raw[3] == 0)
        return false;

    /* Extract fields.
     * Bits 0-2:   flags (3 bits)
     * Bits 3-26:  sensor ID (24 bits)
     * Bits 27-36: pressure raw (10 bits)
     * Bits 37-38: check (2 bits) */
    uint8_t tire_id[3];
    tire_id[0] = ((raw[0] & 0x1F) << 3) | (raw[1] >> 5);
    tire_id[1] = (raw[1] << 3) | (raw[2] >> 5);
    tire_id[2] = (raw[2] << 3) | (raw[3] >> 5);

    uint16_t pressure_raw = ((uint16_t)(raw[3] & 0x1F) << 5) | (raw[4] >> 3);
    float pressure_psi = (float)pressure_raw * 0.2f;

    /* Basic sanity: pressure should be in reasonable range. */
    if (pressure_psi > 100.0f || pressure_psi < 0.0f) return false;

    info->pulses_count = (off + decoded * 2) - info->start_off;

    fieldset_add_bytes(info->fieldset, "Tire ID", tire_id, 3 * 2);
    fieldset_add_float(info->fieldset, "Pressure psi", pressure_psi, 1);
    return true;
}

ProtoViewDecoder SchraderSMD3MA4TPMSDecoder = {
    .name = "Schrader SMD3MA4",
    .decode = decode,
    .get_fields = NULL,
    .build_message = NULL
};
