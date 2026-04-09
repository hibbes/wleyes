// src/wayland.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wayland.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

/* ── layer anchor helper ──────────────────────────────────────────────────── */

static uint32_t get_layer_anchor(int anchor) {
    switch (anchor) {
        case ANCHOR_TOP_LEFT:
            return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
        case ANCHOR_TOP_RIGHT:
            return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        case ANCHOR_BOTTOM_LEFT:
            return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
        case ANCHOR_BOTTOM_RIGHT:
        default:
            return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
    }
}

/* ── tracker layer-surface listeners ─────────────────────────────────────── */

static void tracker_layer_configure(void *data,
                                    struct zwlr_layer_surface_v1 *layer_surface,
                                    uint32_t serial,
                                    uint32_t width, uint32_t height)
{
    struct wleyes_state *state = data;

    state->screen_width  = (int)width;
    state->screen_height = (int)height;

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    /* Set empty input region so input passes through this surface */
    struct wl_region *region = wl_compositor_create_region(state->compositor);
    wl_surface_set_input_region(state->tracker_surface, region);
    wl_region_destroy(region);

    wl_surface_commit(state->tracker_surface);
}

static void tracker_layer_closed(void *data,
                                  struct zwlr_layer_surface_v1 *layer_surface)
{
    (void)layer_surface;
    struct wleyes_state *state = data;
    state->running = false;
}

static const struct zwlr_layer_surface_v1_listener tracker_layer_listener = {
    .configure = tracker_layer_configure,
    .closed    = tracker_layer_closed,
};

/* ── eyes layer-surface listeners ────────────────────────────────────────── */

static void eyes_layer_configure(void *data,
                                  struct zwlr_layer_surface_v1 *layer_surface,
                                  uint32_t serial,
                                  uint32_t width, uint32_t height)
{
    (void)width;
    (void)height;
    struct wleyes_state *state = data;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    wl_surface_commit(state->eyes_surface);
}

static void eyes_layer_closed(void *data,
                               struct zwlr_layer_surface_v1 *layer_surface)
{
    (void)layer_surface;
    struct wleyes_state *state = data;
    state->running = false;
}

static const struct zwlr_layer_surface_v1_listener eyes_layer_listener = {
    .configure = eyes_layer_configure,
    .closed    = eyes_layer_closed,
};

/* ── registry listener ────────────────────────────────────────────────────── */

static void registry_global(void *data, struct wl_registry *registry,
                             uint32_t name, const char *interface,
                             uint32_t version)
{
    struct wleyes_state *state = data;
    (void)version;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name,
                                             &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name,
                                      &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->seat = wl_registry_bind(registry, name,
                                       &wl_seat_interface, 7);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell = wl_registry_bind(registry, name,
                                              &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        /* Bind only the first output unless a specific one was requested.
         * Output-name filtering will be refined in a later task when we
         * actually process wl_output events. */
        if (state->output == NULL) {
            state->output = wl_registry_bind(registry, name,
                                             &wl_output_interface, 3);
        }
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name)
{
    /* Not handled in this stub — a production impl would check if the
     * removed object is one we cached and set the pointer to NULL. */
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

/* ── public API ───────────────────────────────────────────────────────────── */

bool wleyes_setup_surfaces(struct wleyes_state *state) {
    /* ── Tracker surface: fullscreen overlay, input-passthrough ── */
    state->tracker_surface = wl_compositor_create_surface(state->compositor);
    if (!state->tracker_surface) {
        fprintf(stderr, "wleyes: failed to create tracker surface\n");
        return false;
    }

    state->tracker_layer = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell,
        state->tracker_surface,
        state->output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "wleyes-tracker"
    );
    if (!state->tracker_layer) {
        fprintf(stderr, "wleyes: failed to create tracker layer surface\n");
        return false;
    }

    /* Anchor to all 4 edges = fullscreen */
    zwlr_layer_surface_v1_set_anchor(state->tracker_layer,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

    /* exclusive_zone = -1: don't push other surfaces away, extend under panels */
    zwlr_layer_surface_v1_set_exclusive_zone(state->tracker_layer, -1);

    zwlr_layer_surface_v1_add_listener(state->tracker_layer,
                                       &tracker_layer_listener, state);
    wl_surface_commit(state->tracker_surface);

    /* ── Eyes surface: small, positioned ── */
    state->eyes_surface = wl_compositor_create_surface(state->compositor);
    if (!state->eyes_surface) {
        fprintf(stderr, "wleyes: failed to create eyes surface\n");
        return false;
    }

    state->eyes_layer = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell,
        state->eyes_surface,
        state->output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "wleyes"
    );
    if (!state->eyes_layer) {
        fprintf(stderr, "wleyes: failed to create eyes layer surface\n");
        return false;
    }

    zwlr_layer_surface_v1_set_size(state->eyes_layer,
                                   (uint32_t)state->config.width,
                                   (uint32_t)state->config.height);

    zwlr_layer_surface_v1_set_anchor(state->eyes_layer,
                                     get_layer_anchor(state->config.anchor));

    zwlr_layer_surface_v1_set_margin(state->eyes_layer,
        state->config.margin_y,  /* top    */
        state->config.margin_x,  /* right  */
        state->config.margin_y,  /* bottom */
        state->config.margin_x   /* left   */
    );

    zwlr_layer_surface_v1_set_exclusive_zone(state->eyes_layer, 0);

    zwlr_layer_surface_v1_add_listener(state->eyes_layer,
                                       &eyes_layer_listener, state);
    wl_surface_commit(state->eyes_surface);

    return true;
}

bool wleyes_init(struct wleyes_state *state) {
    state->display = wl_display_connect(NULL);
    if (!state->display) {
        fprintf(stderr, "wleyes: failed to connect to Wayland display\n");
        return false;
    }

    state->registry = wl_display_get_registry(state->display);
    if (!state->registry) {
        fprintf(stderr, "wleyes: failed to get Wayland registry\n");
        wl_display_disconnect(state->display);
        state->display = NULL;
        return false;
    }

    wl_registry_add_listener(state->registry, &registry_listener, state);

    /* First roundtrip: populate globals */
    wl_display_roundtrip(state->display);
    /* Second roundtrip: process any events triggered by bindings */
    wl_display_roundtrip(state->display);

    bool ok = true;

    if (!state->compositor) {
        fprintf(stderr, "wleyes: wl_compositor not available\n");
        ok = false;
    }
    if (!state->shm) {
        fprintf(stderr, "wleyes: wl_shm not available\n");
        ok = false;
    }
    if (!state->seat) {
        fprintf(stderr, "wleyes: wl_seat not available\n");
        ok = false;
    }
    if (!state->layer_shell) {
        fprintf(stderr, "wleyes: zwlr_layer_shell_v1 not available "
                        "(compositor does not support wlr-layer-shell)\n");
        ok = false;
    }
    if (!state->output) {
        fprintf(stderr, "wleyes: no wl_output found\n");
        ok = false;
    }

    if (!ok) {
        wleyes_destroy(state);
        return false;
    }

    state->running = true;
    return true;
}

void wleyes_destroy(struct wleyes_state *state) {
    /* Destroy in reverse bind order */
    if (state->eyes_layer)     { zwlr_layer_surface_v1_destroy(state->eyes_layer);    state->eyes_layer    = NULL; }
    if (state->eyes_surface)   { wl_surface_destroy(state->eyes_surface);             state->eyes_surface  = NULL; }
    if (state->tracker_layer)  { zwlr_layer_surface_v1_destroy(state->tracker_layer); state->tracker_layer = NULL; }
    if (state->tracker_surface){ wl_surface_destroy(state->tracker_surface);          state->tracker_surface = NULL; }

    if (state->pointer)      { wl_pointer_destroy(state->pointer);                     state->pointer     = NULL; }
    if (state->layer_shell)  { zwlr_layer_shell_v1_destroy(state->layer_shell);        state->layer_shell = NULL; }
    if (state->output)       { wl_output_destroy(state->output);                       state->output      = NULL; }
    if (state->seat)         { wl_seat_destroy(state->seat);                           state->seat        = NULL; }
    if (state->shm)          { wl_shm_destroy(state->shm);                             state->shm         = NULL; }
    if (state->compositor)   { wl_compositor_destroy(state->compositor);               state->compositor  = NULL; }
    if (state->registry)     { wl_registry_destroy(state->registry);                   state->registry    = NULL; }
    if (state->display)      { wl_display_disconnect(state->display);                  state->display     = NULL; }

    state->running = false;
}
