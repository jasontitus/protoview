/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Modified: TPMS Reader - Focused on US 315 MHz TPMS signal detection. */

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/registry.h>
#include "raw_samples.h"

#define TAG "TPMSReader"
#define PROTOVIEW_RAW_VIEW_DEFAULT_SCALE 100
#define BITMAP_SEEK_NOT_FOUND UINT32_MAX
#define PROTOVIEW_VIEW_PRIVDATA_LEN 64

#define DEBUG_MSG 1

/* ========================= TPMS Sensor Tracking ============================ */

#define TPMS_MAX_SENSORS 32
#define TPMS_ID_MAX_BYTES 8
#define TPMS_DEFAULT_FREQUENCY 315000000

typedef struct {
    uint8_t id[TPMS_ID_MAX_BYTES];
    uint8_t id_len;             /* ID length in bytes. */
    char protocol[24];          /* Decoder name. */
    float pressure_psi;         /* Pressure in PSI. */
    int temperature_f;          /* Temperature in Fahrenheit. */
    bool has_pressure;
    bool has_temperature;
    uint32_t last_seen;         /* Tick when last received. */
    uint32_t rx_count;          /* Number of receptions. */
} TPMSSensor;

typedef struct {
    TPMSSensor sensors[TPMS_MAX_SENSORS];
    uint32_t count;
} TPMSSensorList;

/* ========================= Forward declarations ============================ */

typedef struct ProtoViewApp ProtoViewApp;
typedef struct ProtoViewMsgInfo ProtoViewMsgInfo;
typedef struct ProtoViewFieldSet ProtoViewFieldSet;
typedef struct ProtoViewDecoder ProtoViewDecoder;

/* ============================== enumerations ============================== */

/* Subghz system state */
typedef enum {
    TxRxStateIDLE,
    TxRxStateRx,
    TxRxStateTx,
    TxRxStateSleep,
} TxRxState;

/* Currently active view. */
typedef enum {
    ViewTPMSList,           /* Main view: scanning + sensor list. */
    ViewTPMSDetail,         /* Detail view for a single sensor. */
    ViewFrequencySettings,
    ViewModulationSettings,
    ViewLast,               /* Sentinel to wrap around. */

    /* Special views for the API. */
    ViewGoNext,
    ViewGoPrev,
} ProtoViewCurrentView;

/* ================================== RX/TX ================================= */

typedef struct {
    const char *name;
    const char *id;
    FuriHalSubGhzPreset preset;
    uint8_t *custom;
    uint32_t duration_filter;
} ProtoViewModulation;

extern ProtoViewModulation ProtoViewModulations[];

struct ProtoViewTxRx {
    bool freq_mod_changed;
    TxRxState txrx_state;
    bool debug_timer_sampling;
    uint32_t last_g0_change_time;
    bool last_g0_value;
};

typedef struct ProtoViewTxRx ProtoViewTxRx;

/* ============================== Main app state ============================ */

#define ALERT_MAX_LEN 32
struct ProtoViewApp {
    /* GUI */
    Gui *gui;
    NotificationApp *notification;
    ViewPort *view_port;
    ProtoViewCurrentView current_view;
    FuriMutex *view_updating_mutex;
    int current_subview[ViewLast];
    FuriMessageQueue *event_queue;

    /* Alert state. */
    uint32_t alert_dismiss_time;
    char alert_text[ALERT_MAX_LEN];

    /* Radio related. */
    ProtoViewTxRx *txrx;
    SubGhzSetting *setting;

    /* Generic app state. */
    int running;
    uint32_t signal_bestlen;
    uint32_t signal_last_scan_idx;
    bool signal_decoded;
    ProtoViewMsgInfo *msg_info;
    void *view_privdata;

    /* Raw view state (kept for compatibility with signal.c). */
    uint32_t us_scale;
    uint32_t signal_offset;

    /* Configuration. */
    uint32_t frequency;
    uint8_t modulation;

    /* TPMS sensor tracking. */
    TPMSSensorList sensor_list;
    int selected_sensor;        /* Currently selected sensor in list. */
    int list_scroll_offset;     /* First visible sensor in list. */

    /* Modulation auto-cycling. */
    bool mod_auto_cycle;        /* Auto-cycle through TPMS modulations. */
    uint32_t mod_cycle_counter; /* Timer ticks since last modulation change. */
};

/* =========================== Protocols decoders =========================== */

#define PROTOVIEW_MSG_STR_LEN 32
typedef struct ProtoViewMsgInfo {
    ProtoViewDecoder *decoder;
    ProtoViewFieldSet *fieldset;
    uint32_t start_off;
    uint32_t pulses_count;
    uint32_t short_pulse_dur;
    uint8_t *bits;
    uint32_t bits_bytes;
} ProtoViewMsgInfo;

typedef enum {
    FieldTypeStr,
    FieldTypeSignedInt,
    FieldTypeUnsignedInt,
    FieldTypeBinary,
    FieldTypeHex,
    FieldTypeBytes,
    FieldTypeFloat,
} ProtoViewFieldType;

typedef struct {
    ProtoViewFieldType type;
    uint32_t len;
    char *name;
    union {
        char *str;
        int64_t value;
        uint64_t uvalue;
        uint8_t *bytes;
        float fvalue;
    };
} ProtoViewField;

typedef struct ProtoViewFieldSet {
    ProtoViewField **fields;
    uint32_t numfields;
} ProtoViewFieldSet;

typedef struct ProtoViewDecoder {
    const char *name;
    bool (*decode)(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info);
    void (*get_fields)(ProtoViewFieldSet *fields);
    void (*build_message)(RawSamplesBuffer *samples, ProtoViewFieldSet *fields);
} ProtoViewDecoder;

extern RawSamplesBuffer *RawSamples, *DetectedSamples;

/* app_subghz.c */
void radio_begin(ProtoViewApp* app);
uint32_t radio_rx(ProtoViewApp* app);
void radio_rx_end(ProtoViewApp* app);
void radio_sleep(ProtoViewApp* app);
void raw_sampling_worker_start(ProtoViewApp *app);
void raw_sampling_worker_stop(ProtoViewApp *app);
void radio_tx_signal(ProtoViewApp *app, FuriHalSubGhzAsyncTxCallback data_feeder, void *ctx);
void protoview_rx_callback(bool level, uint32_t duration, void* context);

/* signal.c */
uint32_t duration_delta(uint32_t a, uint32_t b);
void reset_current_signal(ProtoViewApp *app);
void scan_for_signal(ProtoViewApp *app, RawSamplesBuffer *source, uint32_t min_duration);
bool bitmap_get(uint8_t *b, uint32_t blen, uint32_t bitpos);
void bitmap_set(uint8_t *b, uint32_t blen, uint32_t bitpos, bool val);
void bitmap_copy(uint8_t *d, uint32_t dlen, uint32_t doff, uint8_t *s, uint32_t slen, uint32_t soff, uint32_t count);
void bitmap_set_pattern(uint8_t *b, uint32_t blen, uint32_t off, const char *pat);
void bitmap_reverse_bytes_bits(uint8_t *p, uint32_t len);
bool bitmap_match_bits(uint8_t *b, uint32_t blen, uint32_t bitpos, const char *bits);
uint32_t bitmap_seek_bits(uint8_t *b, uint32_t blen, uint32_t startpos, uint32_t maxbits, const char *bits);
bool bitmap_match_bitmap(uint8_t *b1, uint32_t b1len, uint32_t b1off,
                         uint8_t *b2, uint32_t b2len, uint32_t b2off,
                         uint32_t cmplen);
void bitmap_to_string(char *dst, uint8_t *b, uint32_t blen,
                      uint32_t off, uint32_t len);
uint32_t convert_from_line_code(uint8_t *buf, uint64_t buflen, uint8_t *bits, uint32_t len, uint32_t offset, const char *zero_pattern, const char *one_pattern);
uint32_t convert_from_diff_manchester(uint8_t *buf, uint64_t buflen, uint8_t *bits, uint32_t len, uint32_t off, bool previous);
void init_msg_info(ProtoViewMsgInfo *i, ProtoViewApp *app);
void free_msg_info(ProtoViewMsgInfo *i);

/* tpms_sensor.c */
void tpms_sensor_list_init(TPMSSensorList *list);
void tpms_sensor_list_clear(TPMSSensorList *list);
bool tpms_extract_and_store(ProtoViewApp *app);

/* view_tpms_list.c */
void render_view_tpms_list(Canvas *const canvas, ProtoViewApp *app);
void process_input_tpms_list(ProtoViewApp *app, InputEvent input);

/* view_tpms_detail.c */
void render_view_tpms_detail(Canvas *const canvas, ProtoViewApp *app);
void process_input_tpms_detail(ProtoViewApp *app, InputEvent input);

/* view_settings.c */
void render_view_settings(Canvas *const canvas, ProtoViewApp *app);
void process_input_settings(ProtoViewApp *app, InputEvent input);
void view_exit_settings(ProtoViewApp *app);

/* ui.c */
int ui_get_current_subview(ProtoViewApp *app);
void ui_show_available_subviews(Canvas *canvas, ProtoViewApp *app, int last_subview);
bool ui_process_subview_updown(ProtoViewApp *app, InputEvent input, int last_subview);
void ui_show_alert(ProtoViewApp *app, const char *text, uint32_t ttl);
void ui_dismiss_alert(ProtoViewApp *app);
void ui_draw_alert_if_needed(Canvas *canvas, ProtoViewApp *app);
void canvas_draw_str_with_border(Canvas* canvas, uint8_t x, uint8_t y, const char* str, Color text_color, Color border_color);

/* fields.c */
void fieldset_free(ProtoViewFieldSet *fs);
ProtoViewFieldSet *fieldset_new(void);
void fieldset_add_int(ProtoViewFieldSet *fs, const char *name, int64_t val, uint8_t bits);
void fieldset_add_uint(ProtoViewFieldSet *fs, const char *name, uint64_t uval, uint8_t bits);
void fieldset_add_hex(ProtoViewFieldSet *fs, const char *name, uint64_t uval, uint8_t bits);
void fieldset_add_bin(ProtoViewFieldSet *fs, const char *name, uint64_t uval, uint8_t bits);
void fieldset_add_str(ProtoViewFieldSet *fs, const char *name, const char *s, size_t len);
void fieldset_add_bytes(ProtoViewFieldSet *fs, const char *name, const uint8_t *bytes, uint32_t count);
void fieldset_add_float(ProtoViewFieldSet *fs, const char *name, float val, uint32_t digits_after_dot);
const char *field_get_type_name(ProtoViewField *f);
int field_to_string(char *buf, size_t len, ProtoViewField *f);
bool field_set_from_string(ProtoViewField *f, char *buf, size_t len);
bool field_incr_value(ProtoViewField *f, int incr);
void fieldset_copy_matching_fields(ProtoViewFieldSet *dst, ProtoViewFieldSet *src);
void field_set_from_field(ProtoViewField *dst, ProtoViewField *src);

/* crc.c */
uint8_t crc8(const uint8_t *data, size_t len, uint8_t init, uint8_t poly);
uint8_t sum_bytes(const uint8_t *data, size_t len, uint8_t init);
uint8_t xor_bytes(const uint8_t *data, size_t len, uint8_t init);
