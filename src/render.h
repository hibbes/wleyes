#ifndef WLEYES_RENDER_H
#define WLEYES_RENDER_H

#include <stdbool.h>

struct wleyes_state;

#define BLINK_MS 180
#define ROLL_MS  700

bool wleyes_render(struct wleyes_state *state);
/* Ends finished animations; true while one is still running. */
bool wleyes_animating(struct wleyes_state *state);

#endif
