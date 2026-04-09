#ifndef WLEYES_MATH_UTIL_H
#define WLEYES_MATH_UTIL_H

#include <math.h>

static inline void pupil_offset(
        double eye_cx, double eye_cy,
        double cursor_x, double cursor_y,
        double max_radius,
        double *out_x, double *out_y) {
    double dx = cursor_x - eye_cx;
    double dy = cursor_y - eye_cy;
    double dist = sqrt(dx * dx + dy * dy);
    if (dist < 0.001) {
        *out_x = 0;
        *out_y = 0;
        return;
    }
    double clamped = dist < max_radius ? dist : max_radius;
    *out_x = (dx / dist) * clamped;
    *out_y = (dy / dist) * clamped;
}

#endif
