/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Modified: TPMS Reader - Only TPMS protocol decoders are registered. */

#include "app.h"

bool decode_signal(RawSamplesBuffer *s, uint64_t len, ProtoViewMsgInfo *info);

/* =============================================================================
 * TPMS Protocols table.
 * Only TPMS decoders are included for the focused TPMS reader application.
 * ===========================================================================*/

extern ProtoViewDecoder RenaultTPMSDecoder;
extern ProtoViewDecoder ToyotaTPMSDecoder;
extern ProtoViewDecoder SchraderTPMSDecoder;
extern ProtoViewDecoder SchraderEG53MA4TPMSDecoder;
extern ProtoViewDecoder CitroenTPMSDecoder;
extern ProtoViewDecoder FordTPMSDecoder;
extern ProtoViewDecoder HyundaiKiaTPMSDecoder;
extern ProtoViewDecoder GMTPMSDecoder;
extern ProtoViewDecoder PMV107JTPMSDecoder;
extern ProtoViewDecoder Elantra2012TPMSDecoder;
extern ProtoViewDecoder BMWTPMSDecoder;
extern ProtoViewDecoder BMWGen3TPMSDecoder;
extern ProtoViewDecoder PorscheTPMSDecoder;
extern ProtoViewDecoder SchraderSMD3MA4TPMSDecoder;

ProtoViewDecoder *Decoders[] = {
    &PMV107JTPMSDecoder,        /* Toyota Highlander, Camry, Lexus (US). */
    &Elantra2012TPMSDecoder,    /* Hyundai Elantra 2012 / Honda Civic. */
    &BMWTPMSDecoder,            /* BMW Gen4/5 and Audi. */
    &BMWGen3TPMSDecoder,        /* BMW Gen2/Gen3. */
    &PorscheTPMSDecoder,        /* Porsche Boxster/Cayman. */
    &SchraderSMD3MA4TPMSDecoder,/* Schrader SMD3MA4 (Subaru, Nissan, etc). */
    &RenaultTPMSDecoder,
    &ToyotaTPMSDecoder,
    &SchraderTPMSDecoder,
    &SchraderEG53MA4TPMSDecoder,
    &CitroenTPMSDecoder,
    &FordTPMSDecoder,
    &HyundaiKiaTPMSDecoder,
    &GMTPMSDecoder,
    NULL
};

/* =============================================================================
 * Raw signal detection
 * ===========================================================================*/

uint32_t duration_delta(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
}

void reset_current_signal(ProtoViewApp *app) {
    app->signal_bestlen = 0;
    app->signal_offset = 0;
    app->signal_decoded = false;
    raw_samples_reset(DetectedSamples);
    raw_samples_reset(RawSamples);
    free_msg_info(app->msg_info);
    app->msg_info = NULL;
}

#define SEARCH_CLASSES 3
uint32_t search_coherent_signal(RawSamplesBuffer *s, uint32_t idx, uint32_t min_duration) {
    struct {
        uint32_t dur[2];
        uint32_t count[2];
    } classes[SEARCH_CLASSES];

    memset(classes, 0, sizeof(classes));
    uint32_t max_duration = 4000;
    uint32_t len = 0;
    s->short_pulse_dur = 0;

    for (uint32_t j = idx; j < idx + s->total; j++) {
        bool level;
        uint32_t dur;
        raw_samples_get(s, j, &level, &dur);

        if (dur < min_duration || dur > max_duration) break;

        uint32_t k;
        for (k = 0; k < SEARCH_CLASSES; k++) {
            if (classes[k].count[level] == 0) {
                classes[k].dur[level] = dur;
                classes[k].count[level] = 1;
                break;
            } else {
                uint32_t classavg = classes[k].dur[level];
                uint32_t count = classes[k].count[level];
                uint32_t delta = duration_delta(dur, classavg);
                if (delta < classavg / 5) {
                    classavg = ((classavg * count) + dur) / (count + 1);
                    classes[k].dur[level] = classavg;
                    classes[k].count[level]++;
                    break;
                }
            }
        }

        if (k == SEARCH_CLASSES) break;
        len++;
    }

    uint32_t short_dur[2] = {0, 0};
    for (int j = 0; j < SEARCH_CLASSES; j++) {
        for (int level = 0; level < 2; level++) {
            if (classes[j].dur[level] == 0) continue;
            if (classes[j].count[level] < 3) continue;
            if (short_dur[level] == 0 ||
                short_dur[level] > classes[j].dur[level])
            {
                short_dur[level] = classes[j].dur[level];
            }
        }
    }

    if (short_dur[0] == 0) short_dur[0] = short_dur[1];
    if (short_dur[1] == 0) short_dur[1] = short_dur[0];
    s->short_pulse_dur = (short_dur[0] + short_dur[1]) / 2;

    return len;
}

void scan_for_signal(ProtoViewApp *app, RawSamplesBuffer *source, uint32_t min_duration) {
    RawSamplesBuffer *copy = raw_samples_alloc();
    raw_samples_copy(copy, source);

    app->dbg_scan_count++;

    uint32_t minlen = 18;
    uint32_t i = 0;

    while (i < copy->total - 1) {
        uint32_t thislen = search_coherent_signal(copy, i, min_duration);

        if (thislen > minlen) {
            app->dbg_coherent_count++;
            app->dbg_last_signal_len = thislen;
            app->dbg_last_signal_dur = copy->short_pulse_dur;

            ProtoViewMsgInfo *info = malloc(sizeof(ProtoViewMsgInfo));
            init_msg_info(info, app);
            info->short_pulse_dur = copy->short_pulse_dur;

            uint32_t saved_idx = copy->idx;
            raw_samples_center(copy, i);

            app->dbg_decode_try_count++;
            bool decoded = decode_signal(copy, thislen, info);
            if (decoded) app->dbg_decode_ok_count++;

            copy->idx = saved_idx;

            bool oldsignal_not_decoded = app->signal_decoded == false;

            if (oldsignal_not_decoded &&
                (thislen > app->signal_bestlen || decoded))
            {
                free_msg_info(app->msg_info);
                app->msg_info = info;
                app->signal_bestlen = thislen;
                app->signal_decoded = decoded;
                raw_samples_copy(DetectedSamples, copy);
                raw_samples_center(DetectedSamples, i);
                FURI_LOG_E(TAG, "===> Signal updated (%d samples %lu us)",
                    (int)thislen, DetectedSamples->short_pulse_dur);
            } else {
                free_msg_info(info);
            }
        }
        i += thislen ? thislen : 1;
    }
    raw_samples_free(copy);
}

/* =============================================================================
 * Decoding
 * ===========================================================================*/

void bitmap_set(uint8_t *b, uint32_t blen, uint32_t bitpos, bool val) {
    uint32_t byte = bitpos / 8;
    uint32_t bit = 7 - (bitpos & 7);
    if (byte >= blen) return;
    if (val)
        b[byte] |= 1 << bit;
    else
        b[byte] &= ~(1 << bit);
}

bool bitmap_get(uint8_t *b, uint32_t blen, uint32_t bitpos) {
    uint32_t byte = bitpos / 8;
    uint32_t bit = 7 - (bitpos & 7);
    if (byte >= blen) return 0;
    return (b[byte] & (1 << bit)) != 0;
}

void bitmap_copy(uint8_t *d, uint32_t dlen, uint32_t doff,
                 uint8_t *s, uint32_t slen, uint32_t soff,
                 uint32_t count)
{
    if ((doff & 7) == 0 && (soff & 7) == 0) {
        uint32_t didx = doff / 8;
        uint32_t sidx = soff / 8;
        while (count > 8 && didx < dlen && sidx < slen) {
            d[didx++] = s[sidx++];
            count -= 8;
        }
        doff = didx * 8;
        soff = sidx * 8;
    }

    while (count > 8 && (doff & 7) != 0) {
        bool bit = bitmap_get(s, slen, soff++);
        bitmap_set(d, dlen, doff++, bit);
        count--;
    }

    if (count > 8) {
        uint8_t skew = soff % 8;
        uint32_t didx = doff / 8;
        uint32_t sidx = soff / 8;
        while (count > 8 && didx < dlen && sidx < slen) {
            d[didx] = ((s[sidx] << skew) | (s[sidx + 1] >> (8 - skew)));
            sidx++;
            didx++;
            soff += 8;
            doff += 8;
            count -= 8;
        }
    }

    while (count) {
        bool bit = bitmap_get(s, slen, soff++);
        bitmap_set(d, dlen, doff++, bit);
        count--;
    }
}

void bitmap_reverse_bytes_bits(uint8_t *p, uint32_t len) {
    for (uint32_t j = 0; j < len; j++) {
        uint32_t b = p[j];
        b = (b & 0xf0) >> 4 | (b & 0x0f) << 4;
        b = (b & 0xcc) >> 2 | (b & 0x33) << 2;
        b = (b & 0xaa) >> 1 | (b & 0x55) << 1;
        p[j] = b;
    }
}

bool bitmap_match_bits(uint8_t *b, uint32_t blen, uint32_t bitpos, const char *bits) {
    for (size_t j = 0; bits[j]; j++) {
        bool expected = (bits[j] == '1') ? true : false;
        if (bitmap_get(b, blen, bitpos + j) != expected) return false;
    }
    return true;
}

uint32_t bitmap_seek_bits(uint8_t *b, uint32_t blen, uint32_t startpos, uint32_t maxbits, const char *bits) {
    uint32_t endpos = startpos + blen * 8;
    uint32_t end2 = startpos + maxbits;
    if (end2 < endpos) endpos = end2;
    for (uint32_t j = startpos; j < endpos; j++)
        if (bitmap_match_bits(b, blen, j, bits)) return j;
    return BITMAP_SEEK_NOT_FOUND;
}

bool bitmap_match_bitmap(uint8_t *b1, uint32_t b1len, uint32_t b1off,
                         uint8_t *b2, uint32_t b2len, uint32_t b2off,
                         uint32_t cmplen)
{
    for (uint32_t j = 0; j < cmplen; j++) {
        bool bit1 = bitmap_get(b1, b1len, b1off + j);
        bool bit2 = bitmap_get(b2, b2len, b2off + j);
        if (bit1 != bit2) return false;
    }
    return true;
}

void bitmap_to_string(char *dst, uint8_t *b, uint32_t blen,
                      uint32_t off, uint32_t len)
{
    for (uint32_t j = 0; j < len; j++)
        dst[j] = bitmap_get(b, blen, off + j) ? '1' : '0';
    dst[len] = 0;
}

void bitmap_set_pattern(uint8_t *b, uint32_t blen, uint32_t off, const char *pat) {
    uint32_t i = 0;
    while (pat[i]) {
        bitmap_set(b, blen, i + off, pat[i] == '1');
        i++;
    }
}

uint32_t convert_signal_to_bits(uint8_t *b, uint32_t blen, RawSamplesBuffer *s, uint32_t idx, uint32_t count, uint32_t rate) {
    if (rate == 0) return 0;
    uint32_t bitpos = 0;
    for (uint32_t j = 0; j < count; j++) {
        uint32_t dur;
        bool level;
        raw_samples_get(s, j + idx, &level, &dur);

        uint32_t numbits = dur / rate;
        uint32_t rest = dur % rate;
        if (rest > rate / 2) numbits++;
        if (numbits > 1024) numbits = 1024;
        if (numbits == 0) continue;

        while (numbits--) bitmap_set(b, blen, bitpos++, level);
    }
    return bitpos;
}

uint32_t convert_from_line_code(uint8_t *buf, uint64_t buflen, uint8_t *bits, uint32_t len, uint32_t off, const char *zero_pattern, const char *one_pattern)
{
    uint32_t decoded = 0;
    len *= 8;
    while (off < len) {
        bool bitval;
        if (bitmap_match_bits(bits, len, off, zero_pattern)) {
            bitval = false;
            off += strlen(zero_pattern);
        } else if (bitmap_match_bits(bits, len, off, one_pattern)) {
            bitval = true;
            off += strlen(one_pattern);
        } else {
            break;
        }
        bitmap_set(buf, buflen, decoded++, bitval);
        if (decoded / 8 == buflen) break;
    }
    return decoded;
}

uint32_t convert_from_diff_manchester(uint8_t *buf, uint64_t buflen, uint8_t *bits, uint32_t len, uint32_t off, bool previous)
{
    uint32_t decoded = 0;
    len *= 8;
    for (uint32_t j = off; j < len; j += 2) {
        bool b0 = bitmap_get(bits, len, j);
        bool b1 = bitmap_get(bits, len, j + 1);
        if (b0 == previous) break;
        bitmap_set(buf, buflen, decoded++, b0 == b1);
        previous = b1;
        if (decoded / 8 == buflen) break;
    }
    return decoded;
}

/* Proper differential Manchester decoder using 3-sample sliding window.
 * Convention: transition at start = 0, no transition at start = 1.
 * Mid-bit transition is always required (breaks on error).
 * Returns number of bits decoded into buf. */
uint32_t diff_manchester_decode(
    uint8_t *buf, uint32_t buflen,
    uint8_t *bits, uint32_t numbytes, uint32_t off, uint32_t max_bits)
{
    uint32_t decoded = 0;
    uint32_t limit = numbytes * 8;

    if (off >= limit) return 0;
    bool bit = bitmap_get(bits, numbytes, off++);

    while (decoded < max_bits && off < limit) {
        bool bit2 = bitmap_get(bits, numbytes, off++);
        if (bit == bit2) break; /* No mid-bit transition: error. */

        if (off >= limit) break;
        bool bit3 = bitmap_get(bits, numbytes, off++);

        if (bit2 == bit3)
            bitmap_set(buf, buflen, decoded++, true);  /* No start transition → 1. */
        else
            bitmap_set(buf, buflen, decoded++, false); /* Start transition → 0. */
        bit = bit3;
    }
    return decoded;
}

void free_msg_info(ProtoViewMsgInfo *i) {
    if (i == NULL) return;
    fieldset_free(i->fieldset);
    free(i->bits);
    free(i);
}

void init_msg_info(ProtoViewMsgInfo *i, ProtoViewApp *app) {
    UNUSED(app);
    memset(i, 0, sizeof(ProtoViewMsgInfo));
    i->bits = NULL;
    i->fieldset = fieldset_new();
}

bool decode_signal(RawSamplesBuffer *s, uint64_t len, ProtoViewMsgInfo *info) {
    uint32_t bitmap_bits_size = 4096 * 8;
    uint32_t bitmap_size = bitmap_bits_size / 8;

    uint32_t before_samples = 32;
    uint32_t after_samples = 100;

    uint8_t *bitmap = malloc(bitmap_size);
    uint32_t bits = convert_signal_to_bits(bitmap, bitmap_size, s,
        -before_samples, len + before_samples + after_samples,
        s->short_pulse_dur);

    if (DEBUG_MSG) {
        char *str = malloc(1024);
        uint32_t j;
        for (j = 0; j < bits && j < 1023; j++) {
            str[j] = bitmap_get(bitmap, bitmap_size, j) ? '1' : '0';
        }
        str[j] = 0;
        FURI_LOG_E(TAG, "%lu bits sampled: %s", bits, str);
        free(str);
    }

    int j = 0;
    bool decoded = false;
    while (Decoders[j]) {
        uint32_t start_time = furi_get_tick();
        decoded = Decoders[j]->decode(bitmap, bitmap_size, bits, info);
        uint32_t delta = furi_get_tick() - start_time;
        FURI_LOG_E(TAG, "Decoder %s took %lu ms",
            Decoders[j]->name, (unsigned long)delta);
        if (decoded) {
            info->decoder = Decoders[j];
            break;
        }
        j++;
    }

    if (!decoded) {
        FURI_LOG_E(TAG, "No decoding possible");
    } else {
        FURI_LOG_E(TAG, "+++ Decoded %s", info->decoder->name);
        if (info->pulses_count) {
            info->bits_bytes = (info->pulses_count + 7) / 8;
            info->bits = malloc(info->bits_bytes);
            bitmap_copy(info->bits, info->bits_bytes, 0,
                        bitmap, bitmap_size, info->start_off,
                        info->pulses_count);
        }
    }
    free(bitmap);
    return decoded;
}
