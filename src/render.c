#define _GNU_SOURCE
#include "render.h"
#include "wayland.h"
#include "math_util.h"
#include <cairo/cairo.h>
#include <math.h>

bool wleyes_animating(struct wleyes_state *state) {
    if (state->blink_start && state->now_ms - state->blink_start >= BLINK_MS)
        state->blink_start = 0;
    if (state->roll_start && state->now_ms - state->roll_start >= ROLL_MS)
        state->roll_start = 0;
    return state->blink_start || state->roll_start;
}

/* Lid closure 0..1..0 over BLINK_MS */
static double blink_closure(const struct wleyes_state *state) {
    if (!state->blink_start) return 0.0;
    double t = (double)(state->now_ms - state->blink_start) / BLINK_MS;
    if (t >= 1.0) return 0.0;
    return t < 0.5 ? t * 2.0 : (1.0 - t) * 2.0;
}

bool wleyes_render(struct wleyes_state *state) {
    int w = state->config.width;
    int h = state->config.height;
    int stride = w * 4;
    void *data = state->buffer_data[state->current_buffer];

    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        data, CAIRO_FORMAT_ARGB32, w, h, stride);
    cairo_t *cr = cairo_create(surface);

    /* Clear to transparent */
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double ew = w / 2.0;
    double eh = h;
    double sclera_rx = ew * 0.46;
    double sclera_ry = eh * 0.46;
    double pupil_r = sclera_ry * 0.30;
    double max_move = sclera_ry - pupil_r - 0.5;

    double closure = blink_closure(state);
    bool rolling = state->roll_start != 0;
    double roll_angle = 0.0;
    if (rolling) {
        /* One full turn from the top, eased in and out */
        double t = (double)(state->now_ms - state->roll_start) / ROLL_MS;
        if (t > 1.0) t = 1.0;
        double eased = t * t * (3.0 - 2.0 * t);
        roll_angle = state->roll_dir * eased * 2.0 * M_PI;
    }

    for (int i = 0; i < 2; i++) {
        double cx = ew * 0.5 + i * ew;
        double cy = eh * 0.5;

        /* Fully closed lid: just a line */
        if (closure > 0.92) {
            cairo_move_to(cr, cx - sclera_rx, cy);
            cairo_line_to(cr, cx + sclera_rx, cy);
            cairo_set_source_rgb(cr, 0.345, 0.357, 0.439);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
            continue;
        }

        /* Sclera, squashed vertically while blinking */
        double open_ry = sclera_ry * (1.0 - closure);
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_scale(cr, sclera_rx, open_ry);
        cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.345, 0.357, 0.439);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke_preserve(cr);
        cairo_save(cr);
        cairo_clip(cr);   /* pupil stays inside the (half-closed) sclera */

        /* Pupil: follows the cursor, or circles the rim while rolling */
        double px = 0, py = 0;
        if (rolling) {
            px = sin(roll_angle) * max_move;
            py = -cos(roll_angle) * max_move;
        } else {
            double screen_eye_cx = state->eyes_screen_x + cx;
            double screen_eye_cy = state->eyes_screen_y + cy;
            pupil_offset(screen_eye_cx, screen_eye_cy,
                         state->cursor_x, state->cursor_y,
                         max_move, &px, &py);
        }

        cairo_arc(cr, cx + px, cy + py, pupil_r, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_fill(cr);
        cairo_restore(cr);   /* drop the sclera clip */
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return true;
}
