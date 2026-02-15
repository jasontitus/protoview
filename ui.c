/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Modified: TPMS Reader - Simplified UI utilities. */

#include "app.h"

/* =========================== Subview handling ============================== */

int ui_get_current_subview(ProtoViewApp *app) {
    return app->current_subview[app->current_view];
}

void ui_show_available_subviews(Canvas *canvas, ProtoViewApp *app,
                             int last_subview)
{
    int subview = ui_get_current_subview(app);
    if (subview != 0)
        canvas_draw_triangle(canvas, 120, 5, 8, 5, CanvasDirectionBottomToTop);
    if (subview != last_subview - 1)
        canvas_draw_triangle(canvas, 120, 59, 8, 5, CanvasDirectionTopToBottom);
}

bool ui_process_subview_updown(ProtoViewApp *app, InputEvent input, int last_subview) {
    int subview = ui_get_current_subview(app);
    if (input.type == InputTypePress) {
        if (input.key == InputKeyUp) {
            if (subview != 0)
                app->current_subview[app->current_view]--;
            return true;
        } else if (input.key == InputKeyDown) {
            if (subview != last_subview - 1)
                app->current_subview[app->current_view]++;
            return true;
        }
    }
    return false;
}

/* ================================= Alert ================================== */

void ui_show_alert(ProtoViewApp *app, const char *text, uint32_t ttl) {
    app->alert_dismiss_time = furi_get_tick() + furi_ms_to_ticks(ttl);
    snprintf(app->alert_text, ALERT_MAX_LEN, "%s", text);
}

void ui_dismiss_alert(ProtoViewApp *app) {
    app->alert_dismiss_time = 0;
}

void ui_draw_alert_if_needed(Canvas *canvas, ProtoViewApp *app) {
    if (app->alert_dismiss_time == 0) {
        return;
    } else if (app->alert_dismiss_time < furi_get_tick()) {
        ui_dismiss_alert(app);
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    uint8_t w = canvas_string_width(canvas, app->alert_text);
    uint8_t h = 8;
    uint8_t text_x = 64 - (w / 2);
    uint8_t text_y = 32 + 4;
    uint8_t padding = 3;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, text_x - padding, text_y - padding - h,
                    w + padding * 2, h + padding * 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, text_x - padding + 1, text_y - padding - h + 1,
                    w + padding * 2 - 2, h + padding * 2 - 2);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, text_x, text_y, app->alert_text);
}

/* =========================== Canvas extensions ============================ */

void canvas_draw_str_with_border(Canvas* canvas, uint8_t x, uint8_t y,
    const char* str, Color text_color, Color border_color)
{
    struct {
        uint8_t x; uint8_t y;
    } dir[8] = {
        {-1,-1}, {0,-1}, {1,-1}, {1,0},
        {1,1}, {0,1}, {-1,1}, {-1,0}
    };

    canvas_set_color(canvas, border_color);
    for (int j = 0; j < 8; j++)
        canvas_draw_str(canvas, x + dir[j].x, y + dir[j].y, str);
    canvas_set_color(canvas, text_color);
    canvas_draw_str(canvas, x, y, str);
    canvas_set_color(canvas, ColorBlack);
}
