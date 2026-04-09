#define _GNU_SOURCE
#include "render.h"
#include "wayland.h"
#include "math_util.h"
#include <cairo/cairo.h>
#include <math.h>
#include <string.h>

bool wleyes_render(struct wleyes_state *state) {
    int w = state->config.width;
    int h = state->config.height;
    int stride = w * 4;
    int buf_idx = state->current_buffer;
    void *data = state->buffer_data[buf_idx];

    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        data, CAIRO_FORMAT_ARGB32, w, h, stride);
    cairo_t *cr = cairo_create(surface);

    // Clear to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // Eye geometry
    double eye_w = w / 2.0;
    double eye_h = h;
    double sclera_rx = eye_w * 0.42;
    double sclera_ry = eye_h * 0.42;
    double pupil_r = sclera_ry * 0.35;
    double max_move = sclera_ry - pupil_r - 1.0;

    // Compute eye center positions on screen based on anchor
    double eyes_screen_x, eyes_screen_y;
    switch (state->config.anchor) {
    case ANCHOR_TOP_RIGHT:
        eyes_screen_x = state->screen_width - state->config.margin_x - w;
        eyes_screen_y = state->config.margin_y;
        break;
    case ANCHOR_TOP_LEFT:
        eyes_screen_x = state->config.margin_x;
        eyes_screen_y = state->config.margin_y;
        break;
    case ANCHOR_BOTTOM_RIGHT:
        eyes_screen_x = state->screen_width - state->config.margin_x - w;
        eyes_screen_y = state->screen_height - state->config.margin_y - h;
        break;
    case ANCHOR_BOTTOM_LEFT:
        eyes_screen_x = state->config.margin_x;
        eyes_screen_y = state->screen_height - state->config.margin_y - h;
        break;
    default:
        eyes_screen_x = 0;
        eyes_screen_y = 0;
    }

    for (int i = 0; i < 2; i++) {
        double cx = eye_w * 0.5 + i * eye_w;
        double cy = eye_h * 0.5;

        // Sclera (white ellipse)
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_scale(cr, sclera_rx, sclera_ry);
        cairo_arc(cr, 0, 0, 1.0, 0, 2 * M_PI);
        cairo_restore(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.345, 0.357, 0.439); // #585b70
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        // Pupil position
        double screen_eye_cx = eyes_screen_x + cx;
        double screen_eye_cy = eyes_screen_y + cy;
        double px, py;
        pupil_offset(screen_eye_cx, screen_eye_cy,
                     state->cursor_x, state->cursor_y,
                     max_move, &px, &py);

        // Pupil (black circle)
        cairo_arc(cr, cx + px, cy + py, pupil_r, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return true;
}
