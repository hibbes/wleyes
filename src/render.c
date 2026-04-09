#define _GNU_SOURCE
#include "render.h"
#include "wayland.h"
#include "math_util.h"
#include <cairo/cairo.h>
#include <math.h>

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

    for (int i = 0; i < 2; i++) {
        double cx = ew * 0.5 + i * ew;
        double cy = eh * 0.5;

        /* Sclera */
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_scale(cr, sclera_rx, sclera_ry);
        cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.345, 0.357, 0.439);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        /* Pupil */
        double px = 0, py = 0;
        double screen_eye_cx = state->eyes_screen_x + cx;
        double screen_eye_cy = state->eyes_screen_y + cy;
        pupil_offset(screen_eye_cx, screen_eye_cy,
                     state->cursor_x, state->cursor_y,
                     max_move, &px, &py);

        cairo_arc(cr, cx + px, cy + py, pupil_r, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return true;
}
