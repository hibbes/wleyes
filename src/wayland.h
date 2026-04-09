#ifndef WLEYES_WAYLAND_H
#define WLEYES_WAYLAND_H

#include <stdbool.h>
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

enum anchor {
    ANCHOR_TOP_LEFT     = 0,
    ANCHOR_TOP_RIGHT    = 1,
    ANCHOR_BOTTOM_LEFT  = 2,
    ANCHOR_BOTTOM_RIGHT = 3,
};

struct wleyes_config {
    int anchor;
    int margin_x;
    int margin_y;
    int width;
    int height;
    const char *output_name;
};

struct wleyes_state {
    struct wl_display    *display;
    struct wl_registry   *registry;
    struct wl_compositor *compositor;
    struct wl_shm        *shm;
    struct wl_seat       *seat;
    struct wl_pointer    *pointer;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct wl_output     *output;

    /* Tracker surface: fullscreen, transparent, input-passthrough */
    struct wl_surface              *tracker_surface;
    struct zwlr_layer_surface_v1   *tracker_layer;
    int screen_width;
    int screen_height;

    /* Eyes surface: small, positioned */
    struct wl_surface              *eyes_surface;
    struct zwlr_layer_surface_v1   *eyes_layer;

    /* Cursor state */
    double cursor_x;
    double cursor_y;
    bool   cursor_valid;
    bool   needs_redraw;

    /* Config */
    struct wleyes_config config;

    /* Run state */
    bool running;
};

bool wleyes_init(struct wleyes_state *state);
void wleyes_destroy(struct wleyes_state *state);
bool wleyes_setup_surfaces(struct wleyes_state *state);

#endif /* WLEYES_WAYLAND_H */
