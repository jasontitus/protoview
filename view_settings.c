/* TPMS Reader - Settings view.
 * Allows frequency and modulation selection for TPMS scanning. */

#include "app.h"

void render_view_settings(Canvas *const canvas, ProtoViewApp *app) {
    canvas_set_font(canvas, FontPrimary);
    if (app->current_view == ViewFrequencySettings)
        canvas_draw_str_with_border(canvas, 1, 10, "Frequency",
                                    ColorWhite, ColorBlack);
    else
        canvas_draw_str(canvas, 1, 10, "Frequency");

    if (app->current_view == ViewModulationSettings)
        canvas_draw_str_with_border(canvas, 70, 10, "Modulation",
                                    ColorWhite, ColorBlack);
    else
        canvas_draw_str(canvas, 70, 10, "Modulation");

    /* Auto-cycle status. */
    if (app->mod_auto_cycle)
        canvas_draw_str(canvas, 3, 52, "Auto-cycle: ON (long OK: off)");
    else
        canvas_draw_str(canvas, 3, 52, "Auto-cycle: OFF (long OK: on)");

    canvas_draw_str(canvas, 10, 61, "Use up and down to modify");

    /* Show frequency. */
    if (app->current_view == ViewFrequencySettings) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", (double)app->frequency / 1000000);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 30, 40, buf);
    } else if (app->current_view == ViewModulationSettings) {
        int current = app->modulation;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 33, 39, ProtoViewModulations[current].name);
    }
}

void process_input_settings(ProtoViewApp *app, InputEvent input) {
    if (input.type == InputTypeLong && input.key == InputKeyOk) {
        /* Toggle auto-cycle mode. */
        app->mod_auto_cycle = !app->mod_auto_cycle;
        app->mod_cycle_counter = 0;
    } else if (input.type == InputTypePress &&
              (input.key != InputKeyDown || input.key != InputKeyUp))
    {
        if (app->current_view == ViewFrequencySettings) {
            size_t curidx = 0, i;
            size_t count = subghz_setting_get_frequency_count(app->setting);
            bool found = false;

            for (i = 0; i < count; i++) {
                uint32_t freq = subghz_setting_get_frequency(app->setting, i);
                if (freq == app->frequency) {
                    curidx = i;
                    found = true;
                    break;
                }
            }

            if (!found) {
                /* Current frequency (e.g. 315 MHz) not in list.
                 * Jump to first or last entry in the list. */
                if (input.key == InputKeyUp) {
                    curidx = count - 1;
                } else if (input.key == InputKeyDown) {
                    curidx = 0;
                } else {
                    return;
                }
            } else {
                if (input.key == InputKeyUp) {
                    curidx = curidx == 0 ? count - 1 : curidx - 1;
                } else if (input.key == InputKeyDown) {
                    curidx = (curidx + 1) % count;
                } else {
                    return;
                }
            }
            app->frequency = subghz_setting_get_frequency(app->setting, curidx);
        } else if (app->current_view == ViewModulationSettings) {
            uint32_t count = 0;
            uint32_t modid = app->modulation;

            while (ProtoViewModulations[count].name != NULL) count++;
            if (input.key == InputKeyUp) {
                modid = modid == 0 ? count - 1 : modid - 1;
            } else if (input.key == InputKeyDown) {
                modid = (modid + 1) % count;
            } else {
                return;
            }
            app->modulation = modid;
            /* Disable auto-cycle when user manually selects modulation. */
            app->mod_auto_cycle = false;
        }
    } else {
        return;
    }

    app->txrx->freq_mod_changed = true;
}

void view_exit_settings(ProtoViewApp *app) {
    if (app->txrx->freq_mod_changed) {
        FURI_LOG_E(TAG, "Setting frequency/modulation to %lu %s",
                   app->frequency, ProtoViewModulations[app->modulation].name);
        radio_rx_end(app);
        radio_begin(app);
        radio_rx(app);
        app->txrx->freq_mod_changed = false;
    }
}
