/* TPMS Reader - Main list view.
 * Shows scanning status and a list of detected TPMS sensors. */

#include "app.h"

#define LIST_VISIBLE_SENSORS 4
#define LIST_HEADER_HEIGHT 12
#define LIST_LINE_HEIGHT 12
#define LIST_START_Y 22

/* Format a sensor ID as hex string (up to max_chars characters). */
static void format_sensor_id(char *buf, size_t buflen, TPMSSensor *s) {
    size_t pos = 0;
    for (int i = 0; i < s->id_len && pos < buflen - 1; i++) {
        int written = snprintf(buf + pos, buflen - pos, "%02X", s->id[i]);
        if (written > 0) pos += written;
    }
    buf[pos] = 0;
}

/* Get a short protocol abbreviation for display. */
static const char *protocol_short_name(const char *full_name) {
    if (strstr(full_name, "Schrader EG53")) return "SchE";
    if (strstr(full_name, "Schrader")) return "Sch";
    if (strstr(full_name, "Toyota")) return "Toy";
    if (strstr(full_name, "Ford")) return "Ford";
    if (strstr(full_name, "Citroen")) return "Cit";
    if (strstr(full_name, "Renault")) return "Ren";
    if (strstr(full_name, "Hyundai") || strstr(full_name, "Kia")) return "HyKi";
    if (strstr(full_name, "GM")) return "GM";
    return "TPMS";
}

/* Render the main TPMS scanning/list view. */
void render_view_tpms_list(Canvas *const canvas, ProtoViewApp *app) {
    char buf[64];

    /* Header bar with dark background. */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, LIST_HEADER_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);

    /* Show frequency and modulation in header. */
    snprintf(buf, sizeof(buf), "TPMS Reader  %.1fMHz %s",
             (double)app->frequency / 1000000,
             app->mod_auto_cycle ? "Auto" :
                ProtoViewModulations[app->modulation].name);
    canvas_draw_str(canvas, 1, 9, buf);

    canvas_set_color(canvas, ColorBlack);

    if (app->sensor_list.count == 0) {
        /* No sensors found yet - show scanning message. */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter,
                                "Scanning...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter,
                                "Waiting for TPMS signals");

        /* Animated dots to show scanning is active. */
        uint32_t ticks = furi_get_tick();
        int dots = (ticks / 500) % 4;
        char dot_str[5] = "    ";
        for (int i = 0; i < dots; i++) dot_str[i] = '.';
        dot_str[dots] = 0;
        canvas_draw_str(canvas, 99, 44, dot_str);
    } else {
        /* Column headers. */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 12, LIST_START_Y - 2, "ID");
        canvas_draw_str(canvas, 60, LIST_START_Y - 2, "PSI");
        canvas_draw_str(canvas, 92, LIST_START_Y - 2, "Temp");

        /* Clamp scroll offset and selection. */
        if (app->selected_sensor >= (int)app->sensor_list.count)
            app->selected_sensor = (int)app->sensor_list.count - 1;
        if (app->selected_sensor < 0) app->selected_sensor = 0;

        if (app->selected_sensor < app->list_scroll_offset)
            app->list_scroll_offset = app->selected_sensor;
        if (app->selected_sensor >= app->list_scroll_offset + LIST_VISIBLE_SENSORS)
            app->list_scroll_offset = app->selected_sensor - LIST_VISIBLE_SENSORS + 1;

        /* Draw sensor rows. */
        for (int i = 0; i < LIST_VISIBLE_SENSORS; i++) {
            int idx = app->list_scroll_offset + i;
            if (idx >= (int)app->sensor_list.count) break;

            TPMSSensor *s = &app->sensor_list.sensors[idx];
            int y = LIST_START_Y + 8 + i * LIST_LINE_HEIGHT;

            /* Highlight selected row. */
            if (idx == app->selected_sensor) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, 0, y - 9, 128, LIST_LINE_HEIGHT);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            canvas_set_font(canvas, FontSecondary);

            /* Selection cursor. */
            canvas_draw_str(canvas, 1, y, idx == app->selected_sensor ? ">" : " ");

            /* Sensor ID (last 3 bytes = 6 hex chars for compact display). */
            char id_str[20];
            format_sensor_id(id_str, sizeof(id_str), s);
            /* Show last 6 chars of ID for compactness. */
            size_t id_slen = strlen(id_str);
            char *id_display = id_slen > 6 ? id_str + id_slen - 6 : id_str;
            canvas_draw_str(canvas, 10, y, id_display);

            /* Pressure. */
            if (s->has_pressure) {
                snprintf(buf, sizeof(buf), "%.1f", (double)s->pressure_psi);
            } else {
                snprintf(buf, sizeof(buf), "--.-");
            }
            canvas_draw_str(canvas, 55, y, buf);

            /* Temperature. */
            if (s->has_temperature) {
                snprintf(buf, sizeof(buf), "%dF", s->temperature_f);
            } else {
                snprintf(buf, sizeof(buf), "--F");
            }
            canvas_draw_str(canvas, 89, y, buf);

            /* Protocol abbreviation. */
            canvas_draw_str(canvas, 111, y, protocol_short_name(s->protocol));

            /* Restore color for next row. */
            canvas_set_color(canvas, ColorBlack);
        }

        /* Scroll indicators. */
        if (app->list_scroll_offset > 0) {
            canvas_draw_triangle(canvas, 122, LIST_START_Y + 2, 5, 3,
                                 CanvasDirectionBottomToTop);
        }
        if (app->list_scroll_offset + LIST_VISIBLE_SENSORS <
            (int)app->sensor_list.count) {
            canvas_draw_triangle(canvas, 122, 60, 5, 3,
                                 CanvasDirectionTopToBottom);
        }

        /* Status bar. */
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "%lu sensors  OK:view  LongOK:clear",
                 (unsigned long)app->sensor_list.count);
        canvas_draw_str(canvas, 1, 63, buf);
    }
}

/* Handle input for the TPMS list view. */
void process_input_tpms_list(ProtoViewApp *app, InputEvent input) {
    if (input.type == InputTypeShort || input.type == InputTypeRepeat) {
        if (input.key == InputKeyUp) {
            if (app->selected_sensor > 0)
                app->selected_sensor--;
        } else if (input.key == InputKeyDown) {
            if (app->selected_sensor < (int)app->sensor_list.count - 1)
                app->selected_sensor++;
        }
    }

    if (input.type == InputTypeShort && input.key == InputKeyOk) {
        /* Switch to detail view for the selected sensor. */
        if (app->sensor_list.count > 0 &&
            app->selected_sensor < (int)app->sensor_list.count) {
            /* Set the view directly (bypassing the normal left/right
             * navigation) since detail is accessed via OK press. */
            furi_mutex_acquire(app->view_updating_mutex, FuriWaitForever);
            app->current_view = ViewTPMSDetail;
            memset(app->view_privdata, 0, PROTOVIEW_VIEW_PRIVDATA_LEN);
            furi_mutex_release(app->view_updating_mutex);
        }
    }

    if (input.type == InputTypeLong && input.key == InputKeyOk) {
        /* Clear the sensor list. */
        tpms_sensor_list_clear(&app->sensor_list);
        app->selected_sensor = 0;
        app->list_scroll_offset = 0;
        ui_show_alert(app, "List cleared", 800);
    }
}
