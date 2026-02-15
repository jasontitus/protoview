/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Modified: TPMS Reader - Focused on US 315 MHz TPMS signal detection. */

#include "app.h"

RawSamplesBuffer *RawSamples, *DetectedSamples;

/* The rendering callback dispatches to the active view's render function. */
static void render_callback(Canvas *const canvas, void *ctx) {
    ProtoViewApp *app = ctx;
    furi_mutex_acquire(app->view_updating_mutex, FuriWaitForever);

    /* Clear screen. */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, 127, 63);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    switch(app->current_view) {
    case ViewTPMSList:
        render_view_tpms_list(canvas, app);
        break;
    case ViewTPMSDetail:
        render_view_tpms_detail(canvas, app);
        break;
    case ViewFrequencySettings:
    case ViewModulationSettings:
        render_view_settings(canvas, app);
        break;
    default:
        furi_crash(TAG " Invalid view selected");
        break;
    }

    ui_draw_alert_if_needed(canvas, app);
    furi_mutex_release(app->view_updating_mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    ProtoViewApp *app = ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

/* Switch between views. */
static void app_switch_view(ProtoViewApp *app, ProtoViewCurrentView switchto) {
    furi_mutex_acquire(app->view_updating_mutex, FuriWaitForever);

    ProtoViewCurrentView old = app->current_view;
    if (switchto == ViewGoNext) {
        app->current_view++;
        if (app->current_view == ViewLast) app->current_view = 0;
    } else if (switchto == ViewGoPrev) {
        if (app->current_view == 0)
            app->current_view = ViewLast-1;
        else
            app->current_view--;
    } else {
        app->current_view = switchto;
    }
    ProtoViewCurrentView newview = app->current_view;

    /* Handle settings exit. */
    if ((old == ViewFrequencySettings && newview != ViewModulationSettings) ||
        (old == ViewModulationSettings && newview != ViewFrequencySettings))
        view_exit_settings(app);

    memset(app->view_privdata, 0, PROTOVIEW_VIEW_PRIVDATA_LEN);
    app->current_subview[old] = 0;
    ui_dismiss_alert(app);

    furi_mutex_release(app->view_updating_mutex);
}

/* Find the index of a TPMS modulation preset. Returns the first
 * TPMS preset found, or 0 if none. */
static uint8_t find_tpms_modulation(void) {
    int i = 0;
    while (ProtoViewModulations[i].name != NULL) {
        if (strstr(ProtoViewModulations[i].name, "TPMS") != NULL)
            return i;
        i++;
    }
    return 0;
}

/* Allocate and initialize the app. */
ProtoViewApp* protoview_app_alloc() {
    ProtoViewApp *app = malloc(sizeof(ProtoViewApp));

    RawSamples = raw_samples_alloc();
    DetectedSamples = raw_samples_alloc();

    app->setting = subghz_setting_alloc();
    subghz_setting_load(app->setting, EXT_PATH("subghz/assets/setting_user"));

    /* Storage for persisting TPMS data. */
    app->storage = furi_record_open(RECORD_STORAGE);

    /* GUI setup. */
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->alert_dismiss_time = 0;
    app->current_view = ViewTPMSList;
    app->view_updating_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    for (int j = 0; j < ViewLast; j++) app->current_subview[j] = 0;
    app->view_privdata = malloc(PROTOVIEW_VIEW_PRIVDATA_LEN);
    memset(app->view_privdata, 0, PROTOVIEW_VIEW_PRIVDATA_LEN);

    /* Signal detection state. */
    app->signal_bestlen = 0;
    app->signal_last_scan_idx = 0;
    app->signal_decoded = false;
    app->us_scale = PROTOVIEW_RAW_VIEW_DEFAULT_SCALE;
    app->signal_offset = 0;
    app->msg_info = NULL;

    /* Radio. */
    app->txrx = malloc(sizeof(ProtoViewTxRx));
    app->txrx->freq_mod_changed = false;
    app->txrx->debug_timer_sampling = false;
    app->txrx->last_g0_change_time = DWT->CYCCNT;
    app->txrx->last_g0_value = false;

    /* Always start on 315 MHz (US TPMS). The CC1101 supports this
     * frequency regardless of the Flipper's setting_user list. */
    app->frequency = TPMS_DEFAULT_FREQUENCY;
    app->modulation = find_tpms_modulation();

    /* TPMS sensor list. */
    tpms_sensor_list_init(&app->sensor_list);
    app->selected_sensor = 0;
    app->list_scroll_offset = 0;

    /* Modulation auto-cycling. */
    app->mod_auto_cycle = true;
    app->mod_cycle_counter = 0;

    furi_hal_power_suppress_charge_enter();
    app->running = 1;

    return app;
}

void protoview_app_free(ProtoViewApp *app) {
    furi_assert(app);

    radio_sleep(app);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->view_updating_mutex);
    app->gui = NULL;

    subghz_setting_free(app->setting);
    free(app->txrx);
    free(app->view_privdata);

    raw_samples_free(RawSamples);
    raw_samples_free(DetectedSamples);
    furi_hal_power_suppress_charge_exit();

    free(app);
}

/* Get the next TPMS modulation index for auto-cycling.
 * Cycles through all modulations that have "TPMS" in their name. */
static uint8_t next_tpms_modulation(uint8_t current) {
    uint8_t start = current;
    uint8_t idx = current;
    do {
        idx++;
        if (ProtoViewModulations[idx].name == NULL) idx = 0;
        if (strstr(ProtoViewModulations[idx].name, "TPMS") != NULL)
            return idx;
    } while (idx != start);
    return current; /* No other TPMS modulation found. */
}

/* Called periodically for signal processing. After detecting a TPMS signal,
 * extract the sensor data and reset for the next detection. */
static void timer_callback(void *ctx) {
    ProtoViewApp *app = ctx;
    uint32_t delta, lastidx = app->signal_last_scan_idx;

    /* Only scan when the buffer has filled 50% more since last scan. */
    if (lastidx < RawSamples->idx) {
        delta = RawSamples->idx - lastidx;
    } else {
        delta = RawSamples->total - lastidx + RawSamples->idx;
    }
    if (delta < RawSamples->total/2) return;
    app->signal_last_scan_idx = RawSamples->idx;

    scan_for_signal(app, RawSamples,
                    ProtoViewModulations[app->modulation].duration_filter);

    /* If a signal was decoded, try to extract TPMS data and add to the
     * sensor list, then reset detection for the next signal. */
    if (app->signal_decoded && app->msg_info) {
        tpms_extract_and_store(app);
        /* Reset detection state (but not the raw buffer). */
        app->signal_bestlen = 0;
        app->signal_decoded = false;
        raw_samples_reset(DetectedSamples);
        free_msg_info(app->msg_info);
        app->msg_info = NULL;
    }

    /* Auto-cycle TPMS modulations every ~3 seconds (24 ticks at 8/sec). */
    if (app->mod_auto_cycle) {
        app->mod_cycle_counter++;
        if (app->mod_cycle_counter >= 24) {
            app->mod_cycle_counter = 0;
            uint8_t next = next_tpms_modulation(app->modulation);
            if (next != app->modulation) {
                app->modulation = next;
                radio_rx_end(app);
                radio_begin(app);
                radio_rx(app);
            }
        }
    }
}

/* App entry point. */
int32_t protoview_app_entry(void* p) {
    UNUSED(p);
    ProtoViewApp *app = protoview_app_alloc();

    FuriTimer *timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 8);

    /* Start listening immediately. */
    radio_begin(app);
    radio_rx(app);

    InputEvent input;
    while(app->running) {
        FuriStatus qstat = furi_message_queue_get(app->event_queue, &input, 100);
        if (qstat == FuriStatusOk) {
            if (DEBUG_MSG) FURI_LOG_E(TAG, "Input: type %d key %u",
                    input.type, input.key);

            /* Navigation: Back exits from sub-views or quits. */
            if (input.type == InputTypeShort && input.key == InputKeyBack) {
                if (app->current_view == ViewTPMSDetail) {
                    /* Return to list from detail. */
                    app_switch_view(app, ViewTPMSList);
                } else if (app->current_view != ViewTPMSList) {
                    app_switch_view(app, ViewTPMSList);
                } else {
                    ui_show_alert(app, "Long press to exit", 1000);
                }
            } else if (input.type == InputTypeLong && input.key == InputKeyBack) {
                app->running = 0;
            } else if (input.type == InputTypeShort &&
                       input.key == InputKeyRight &&
                       ui_get_current_subview(app) == 0 &&
                       app->current_view != ViewTPMSDetail)
            {
                app_switch_view(app, ViewGoNext);
            } else if (input.type == InputTypeShort &&
                       input.key == InputKeyLeft &&
                       ui_get_current_subview(app) == 0 &&
                       app->current_view != ViewTPMSDetail)
            {
                app_switch_view(app, ViewGoPrev);
            } else {
                /* Pass input to the active view. */
                switch(app->current_view) {
                case ViewTPMSList:
                    process_input_tpms_list(app, input);
                    break;
                case ViewTPMSDetail:
                    process_input_tpms_detail(app, input);
                    break;
                case ViewFrequencySettings:
                case ViewModulationSettings:
                    process_input_settings(app, input);
                    break;
                default:
                    furi_crash(TAG " Invalid view selected");
                    break;
                }
            }
        } else {
            if (DEBUG_MSG) {
                static int c = 0; c++;
                if (!(c % 20)) FURI_LOG_E(TAG, "Loop timeout");
            }
        }
        view_port_update(app->view_port);
    }

    if (app->txrx->txrx_state == TxRxStateRx) {
        FURI_LOG_E(TAG, "Putting CC1101 to sleep before exiting.");
        radio_rx_end(app);
        radio_sleep(app);
    }

    furi_timer_free(timer);
    protoview_app_free(app);
    return 0;
}
