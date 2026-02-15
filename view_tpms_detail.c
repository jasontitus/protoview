/* TPMS Reader - Sensor detail view.
 * Shows full information for a single TPMS sensor. */

#include "app.h"

/* Format a sensor ID as full hex string. */
static void format_full_id(char *buf, size_t buflen, TPMSSensor *s) {
    size_t pos = 0;
    for (int i = 0; i < s->id_len && pos < buflen - 1; i++) {
        int written = snprintf(buf + pos, buflen - pos, "%02X", s->id[i]);
        if (written > 0) pos += written;
    }
    buf[pos] = 0;
}

/* Render the detail view for the selected TPMS sensor. */
void render_view_tpms_detail(Canvas *const canvas, ProtoViewApp *app) {
    char buf[64];

    if (app->selected_sensor >= (int)app->sensor_list.count) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 20, 32, "No sensor selected");
        return;
    }

    TPMSSensor *s = &app->sensor_list.sensors[app->selected_sensor];

    /* Title bar. */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Sensor %d/%lu  %s",
             app->selected_sensor + 1,
             (unsigned long)app->sensor_list.count,
             s->protocol);
    canvas_draw_str(canvas, 1, 9, buf);

    canvas_set_color(canvas, ColorBlack);
    int y = 22;
    int line_h = 10;

    /* Tire ID (full). */
    char id_str[20];
    format_full_id(id_str, sizeof(id_str), s);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "ID: %s", id_str);
    canvas_draw_str(canvas, 2, y, buf);
    y += line_h;

    /* Pressure in PSI. */
    if (s->has_pressure) {
        snprintf(buf, sizeof(buf), "Pressure: %.1f PSI (%.0f kPa)",
                 (double)s->pressure_psi,
                 (double)(s->pressure_psi / 0.14503774f));
    } else {
        snprintf(buf, sizeof(buf), "Pressure: --");
    }
    canvas_draw_str(canvas, 2, y, buf);
    y += line_h;

    /* Temperature in F and C. */
    if (s->has_temperature) {
        int temp_c = (s->temperature_f - 32) * 5 / 9;
        snprintf(buf, sizeof(buf), "Temp: %dF (%dC)", s->temperature_f, temp_c);
    } else {
        snprintf(buf, sizeof(buf), "Temp: --");
    }
    canvas_draw_str(canvas, 2, y, buf);
    y += line_h;

    /* Reception count and last seen. */
    uint32_t now = furi_get_tick();
    uint32_t elapsed_ms = (now - s->last_seen) * 1000 / furi_kernel_get_tick_frequency();
    uint32_t elapsed_sec = elapsed_ms / 1000;

    snprintf(buf, sizeof(buf), "Received: %lux, %lus ago",
             (unsigned long)s->rx_count,
             (unsigned long)elapsed_sec);
    canvas_draw_str(canvas, 2, y, buf);

    /* Footer. */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 63, "BACK:list  </>:prev/next");
}

/* Handle input for the detail view. */
void process_input_tpms_detail(ProtoViewApp *app, InputEvent input) {
    if (input.type == InputTypeShort) {
        if (input.key == InputKeyLeft) {
            /* Previous sensor. */
            if (app->selected_sensor > 0)
                app->selected_sensor--;
        } else if (input.key == InputKeyRight) {
            /* Next sensor. */
            if (app->selected_sensor < (int)app->sensor_list.count - 1)
                app->selected_sensor++;
        }
    }
}
