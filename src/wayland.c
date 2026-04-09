// src/wayland.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wayland.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

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
