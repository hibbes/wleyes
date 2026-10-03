#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <poll.h>
#include <time.h>
#include <libudev.h>
#include <libinput.h>

#include "wayland.h"

/* ── compute eyes screen position from anchor/margin ─────────────────────── */

static void compute_eyes_screen_pos(struct wleyes_state *state) {
    int w = state->config.width;
    int h = state->config.height;
    switch (state->config.anchor) {
    case ANCHOR_TOP_RIGHT:
        state->eyes_screen_x = state->screen_width - state->config.margin_x - w;
        state->eyes_screen_y = state->config.margin_y;
        break;
    case ANCHOR_TOP_LEFT:
        state->eyes_screen_x = state->config.margin_x;
        state->eyes_screen_y = state->config.margin_y;
        break;
    case ANCHOR_BOTTOM_RIGHT:
        state->eyes_screen_x = state->screen_width - state->config.margin_x - w;
        state->eyes_screen_y = state->screen_height - state->config.margin_y - h;
        break;
    case ANCHOR_BOTTOM_LEFT:
        state->eyes_screen_x = state->config.margin_x;
        state->eyes_screen_y = state->screen_height - state->config.margin_y - h;
        break;
    }
}

/* ── pointer listener on eyes surface (for recalibration) ────────────────── */

static void pointer_enter(void *data, struct wl_pointer *pointer,
        uint32_t serial, struct wl_surface *surface,
        wl_fixed_t sx, wl_fixed_t sy) {
    (void)pointer; (void)serial;
    struct wleyes_state *state = data;

    if (surface == state->cal_surface) {
        /* Calibration surface is fullscreen — surface coords = screen coords */
        state->cursor_x = wl_fixed_to_double(sx);
        state->cursor_y = wl_fixed_to_double(sy);
        state->calibrated = true;
        state->needs_redraw = true;
    } else if (surface == state->eyes_surface) {
        /* Eyes surface — recalibrate from known position */
        state->cursor_x = state->eyes_screen_x + wl_fixed_to_double(sx);
        state->cursor_y = state->eyes_screen_y + wl_fixed_to_double(sy);
        state->needs_redraw = true;
    }
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
        uint32_t serial, struct wl_surface *surface) {
    (void)data; (void)pointer; (void)serial; (void)surface;
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
        uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    (void)pointer; (void)time;
    struct wleyes_state *state = data;
    /* We don't know which surface this is for, but the coords are
     * surface-local. During calibration the cal_surface is fullscreen
     * so coords = screen coords. After calibration, only the eyes
     * surface receives motion (cal is destroyed). */
    if (!state->calibrated && state->cal_surface) {
        state->cursor_x = wl_fixed_to_double(sx);
        state->cursor_y = wl_fixed_to_double(sy);
        state->calibrated = true;
    } else {
        state->cursor_x = state->eyes_screen_x + wl_fixed_to_double(sx);
        state->cursor_y = state->eyes_screen_y + wl_fixed_to_double(sy);
    }
    state->needs_redraw = true;
}

static void pointer_button(void *data, struct wl_pointer *p,
        uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void)data; (void)p; (void)serial; (void)time; (void)button; (void)state;
}

static void pointer_axis(void *data, struct wl_pointer *p,
        uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data; (void)p; (void)time; (void)axis; (void)value;
}

static void pointer_frame(void *data, struct wl_pointer *p) {
    (void)data; (void)p;
}
static void pointer_axis_source(void *data, struct wl_pointer *p, uint32_t s) {
    (void)data; (void)p; (void)s;
}
static void pointer_axis_stop(void *data, struct wl_pointer *p, uint32_t t, uint32_t a) {
    (void)data; (void)p; (void)t; (void)a;
}
static void pointer_axis_discrete(void *data, struct wl_pointer *p, uint32_t a, int32_t d) {
    (void)data; (void)p; (void)a; (void)d;
}
static void pointer_axis_value120(void *data, struct wl_pointer *p, uint32_t a, int32_t v) {
    (void)data; (void)p; (void)a; (void)v;
}
static void pointer_axis_relative_direction(void *data, struct wl_pointer *p, uint32_t a, uint32_t d) {
    (void)data; (void)p; (void)a; (void)d;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter  = pointer_enter,
    .leave  = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis   = pointer_axis,
    .frame  = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
    .axis_value120 = pointer_axis_value120,
    .axis_relative_direction = pointer_axis_relative_direction,
};

/* ── eyes layer-surface listeners ────────────────────────────────────────── */

static void eyes_layer_configure(void *data,
                                  struct zwlr_layer_surface_v1 *layer_surface,
                                  uint32_t serial,
                                  uint32_t width, uint32_t height)
{
    (void)width; (void)height;
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

/* ── output listener (to get screen size) ────────────────────────────────── */

static void output_geometry(void *data, struct wl_output *output,
        int32_t x, int32_t y, int32_t pw, int32_t ph,
        int32_t subpixel, const char *make, const char *model, int32_t transform) {
    (void)data; (void)output; (void)x; (void)y; (void)pw; (void)ph;
    (void)subpixel; (void)make; (void)model; (void)transform;
}

static void output_mode(void *data, struct wl_output *output,
        uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    (void)output; (void)refresh;
    struct wleyes_state *state = data;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        state->screen_width = width;
        state->screen_height = height;
    }
}

static void output_done(void *data, struct wl_output *output) {
    (void)data; (void)output;
}

static void output_scale(void *data, struct wl_output *output, int32_t factor) {
    (void)data; (void)output; (void)factor;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode     = output_mode,
    .done     = output_done,
    .scale    = output_scale,
};

/* ── registry listener ───────────────────────────────────────────────────── */

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
        if (!state->seat)
            state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell = wl_registry_bind(registry, name,
                                              &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        if (state->output == NULL) {
            state->output = wl_registry_bind(registry, name,
                                             &wl_output_interface, 3);
            wl_output_add_listener(state->output, &output_listener, state);
        }
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name)
{
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_global,
    .global_remove = registry_global_remove,
};

/* ── layer anchor helper ─────────────────────────────────────────────────── */

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

/* ── public API ──────────────────────────────────────────────────────────── */

bool wleyes_init(struct wleyes_state *state) {
    state->li = NULL;
    state->li_fd = -1;

    state->display = wl_display_connect(NULL);
    if (!state->display) {
        fprintf(stderr, "wleyes: failed to connect to Wayland display\n");
        return false;
    }

    state->registry = wl_display_get_registry(state->display);
    wl_registry_add_listener(state->registry, &registry_listener, state);
    wl_display_roundtrip(state->display);
    wl_display_roundtrip(state->display);

    if (!state->compositor || !state->shm || !state->layer_shell) {
        fprintf(stderr, "wleyes: missing required Wayland globals\n");
        return false;
    }

    /* Set up pointer listener for recalibration */
    if (state->seat) {
        state->pointer = wl_seat_get_pointer(state->seat);
        wl_pointer_add_listener(state->pointer, &pointer_listener, state);
    }

    state->running = true;
    return true;
}

bool wleyes_setup_surface(struct wleyes_state *state) {
    state->eyes_surface = wl_compositor_create_surface(state->compositor);
    if (!state->eyes_surface) return false;

    state->eyes_layer = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell,
        state->eyes_surface,
        state->output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "wleyes"
    );
    if (!state->eyes_layer) return false;

    zwlr_layer_surface_v1_set_size(state->eyes_layer,
                                   (uint32_t)state->config.width,
                                   (uint32_t)state->config.height);

    zwlr_layer_surface_v1_set_anchor(state->eyes_layer,
                                     get_layer_anchor(state->config.anchor));

    zwlr_layer_surface_v1_set_margin(state->eyes_layer,
        state->config.margin_y,
        state->config.margin_x,
        state->config.margin_y,
        state->config.margin_x);

    zwlr_layer_surface_v1_set_exclusive_zone(state->eyes_layer, -1);

    /* Keep default (full) input region — pointer events on eyes recalibrate cursor */

    zwlr_layer_surface_v1_add_listener(state->eyes_layer,
                                       &eyes_layer_listener, state);
    wl_surface_commit(state->eyes_surface);
    wl_display_roundtrip(state->display);

    /* Compute where the eyes are on screen */
    compute_eyes_screen_pos(state);

    return true;
}

/* ── SHM buffer management ───────────────────────────────────────────────── */

static int create_shm_file(off_t size) {
    int fd = shm_open("/wleyes-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        shm_unlink("/wleyes-shm");
        fd = shm_open("/wleyes-shm", O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd < 0) return -1;
    }
    shm_unlink("/wleyes-shm");
    if (ftruncate(fd, size) < 0) { close(fd); return -1; }
    return fd;
}

bool wleyes_create_buffers(struct wleyes_state *state) {
    int width  = state->config.width;
    int height = state->config.height;
    int stride = width * 4;
    int buf_size = stride * height;
    int total = buf_size * 2;

    state->buffer_size = buf_size;

    int fd = create_shm_file((off_t)total);
    if (fd < 0) return false;

    void *data = mmap(NULL, (size_t)total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) { close(fd); return false; }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, total);
    state->buffers[0] = wl_shm_pool_create_buffer(pool, 0,
        width, height, stride, WL_SHM_FORMAT_ARGB8888);
    state->buffers[1] = wl_shm_pool_create_buffer(pool, buf_size,
        width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    state->buffer_data[0] = data;
    state->buffer_data[1] = (char *)data + buf_size;
    state->current_buffer = 0;
    state->buf_width = width;
    state->buf_height = height;
    return true;
}

/* ── calibration surface (fullscreen, temporary) ─────────────────────────── */

static void cal_layer_configure(void *data,
        struct zwlr_layer_surface_v1 *layer_surface,
        uint32_t serial, uint32_t width, uint32_t height) {
    struct wleyes_state *state = data;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    /* Create a fullscreen transparent buffer — input region is clipped to
     * buffer size, so we need a real full-size buffer for the pointer to
     * enter anywhere on screen. */
    if (!state->cal_buffer && width > 0 && height > 0) {
        int stride = (int)width * 4;
        int size = stride * (int)height;

        int fd = shm_open("/wleyes-cal", O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd < 0) { shm_unlink("/wleyes-cal"); fd = shm_open("/wleyes-cal", O_RDWR | O_CREAT | O_EXCL, 0600); }
        if (fd >= 0) {
            shm_unlink("/wleyes-cal");
            ftruncate(fd, size);
            void *p = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (p != MAP_FAILED) {
                memset(p, 0, (size_t)size);  /* all transparent */
                struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
                state->cal_buffer = wl_shm_pool_create_buffer(pool, 0,
                    (int)width, (int)height, stride, WL_SHM_FORMAT_ARGB8888);
                wl_shm_pool_destroy(pool);
                munmap(p, (size_t)size);
            }
            close(fd);
        }
    }
    if (state->cal_buffer) {
        wl_surface_attach(state->cal_surface, state->cal_buffer, 0, 0);
        wl_surface_commit(state->cal_surface);
    }
}

static void cal_layer_closed(void *data, struct zwlr_layer_surface_v1 *s) {
    (void)data; (void)s;
}

static const struct zwlr_layer_surface_v1_listener cal_layer_listener = {
    .configure = cal_layer_configure,
    .closed    = cal_layer_closed,
};

bool wleyes_setup_calibration_surface(struct wleyes_state *state) {
    state->cal_surface = wl_compositor_create_surface(state->compositor);
    if (!state->cal_surface) return false;

    state->cal_layer = zwlr_layer_shell_v1_get_layer_surface(
        state->layer_shell, state->cal_surface, state->output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "wleyes-cal");

    /* Fullscreen: anchor all edges */
    zwlr_layer_surface_v1_set_anchor(state->cal_layer,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(state->cal_layer, -1);

    zwlr_layer_surface_v1_add_listener(state->cal_layer, &cal_layer_listener, state);
    wl_surface_commit(state->cal_surface);
    wl_display_roundtrip(state->display);
    wl_display_roundtrip(state->display);

    return true;
}

void wleyes_destroy_calibration_surface(struct wleyes_state *state) {
    if (state->cal_buffer) { wl_buffer_destroy(state->cal_buffer); state->cal_buffer = NULL; }
    if (state->cal_layer) { zwlr_layer_surface_v1_destroy(state->cal_layer); state->cal_layer = NULL; }
    if (state->cal_surface) { wl_surface_destroy(state->cal_surface); state->cal_surface = NULL; }
    wl_display_roundtrip(state->display);
}

/* ── libinput ─────────────────────────────────────────────────────────────── */

static int open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;
    return open(path, flags);
}

static void close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

static const struct libinput_interface li_iface = {
    .open_restricted  = open_restricted,
    .close_restricted = close_restricted,
};

static void li_log_handler(struct libinput *li,
        enum libinput_log_priority priority, const char *format, va_list args) {
    (void)li; (void)priority; (void)format; (void)args;
}

bool wleyes_open_libinput(struct wleyes_state *state) {
    struct udev *udev = udev_new();
    if (!udev) return false;

    state->li = libinput_udev_create_context(&li_iface, NULL, udev);
    udev_unref(udev);
    if (!state->li) return false;

    libinput_log_set_handler(state->li, li_log_handler);
    libinput_log_set_priority(state->li, LIBINPUT_LOG_PRIORITY_ERROR);

    if (libinput_udev_assign_seat(state->li, "seat0") != 0) {
        libinput_unref(state->li);
        state->li = NULL;
        return false;
    }

    state->li_fd = libinput_get_fd(state->li);

    /* Drain initial events */
    libinput_dispatch(state->li);
    struct libinput_event *ev;
    while ((ev = libinput_get_event(state->li)) != NULL)
        libinput_event_destroy(ev);

    return true;
}

void wleyes_process_libinput(struct wleyes_state *state) {
    libinput_dispatch(state->li);

    struct libinput_event *ev;
    while ((ev = libinput_get_event(state->li)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(ev);
        if (type == LIBINPUT_EVENT_POINTER_MOTION) {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
            double dx = libinput_event_pointer_get_dx(p);
            double dy = libinput_event_pointer_get_dy(p);

            state->cursor_x += dx;
            state->cursor_y += dy;

            if (state->cursor_x < 0) state->cursor_x = 0;
            if (state->cursor_x >= state->screen_width)
                state->cursor_x = state->screen_width - 1;
            if (state->cursor_y < 0) state->cursor_y = 0;
            if (state->cursor_y >= state->screen_height)
                state->cursor_y = state->screen_height - 1;

            state->needs_redraw = true;
        } else if (type == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE) {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
            state->cursor_x = libinput_event_pointer_get_absolute_x_transformed(
                p, state->screen_width);
            state->cursor_y = libinput_event_pointer_get_absolute_y_transformed(
                p, state->screen_height);
            state->needs_redraw = true;
        } else if (type == LIBINPUT_EVENT_POINTER_BUTTON && state->config.blink) {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
            if (libinput_event_pointer_get_button_state(p) == LIBINPUT_BUTTON_STATE_PRESSED) {
                state->blink_start = state->now_ms;
                state->needs_redraw = true;
            }
        } else if (type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL && state->config.roll) {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
            if (libinput_event_pointer_has_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
                double v = libinput_event_pointer_get_scroll_value_v120(
                    p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
                /* Start a roll unless one is already running */
                if (v != 0 && !state->roll_start) {
                    state->roll_start = state->now_ms;
                    state->roll_dir = v > 0 ? 1 : -1;
                    state->needs_redraw = true;
                }
            }
        }
        libinput_event_destroy(ev);
    }
}

/* ── cleanup ─────────────────────────────────────────────────────────────── */

void wleyes_destroy(struct wleyes_state *state) {
    if (state->li) { libinput_unref(state->li); state->li = NULL; }

    if (state->buffers[0]) { wl_buffer_destroy(state->buffers[0]); state->buffers[0] = NULL; }
    if (state->buffers[1]) { wl_buffer_destroy(state->buffers[1]); state->buffers[1] = NULL; }
    if (state->buffer_data[0]) {
        munmap(state->buffer_data[0], (size_t)state->buffer_size * 2);
        state->buffer_data[0] = NULL;
    }

    if (state->eyes_layer)   { zwlr_layer_surface_v1_destroy(state->eyes_layer); }
    if (state->eyes_surface) { wl_surface_destroy(state->eyes_surface); }

    if (state->pointer)     { wl_pointer_destroy(state->pointer); }
    if (state->seat)        { wl_seat_destroy(state->seat); }
    if (state->layer_shell) { zwlr_layer_shell_v1_destroy(state->layer_shell); }
    if (state->output)      { wl_output_destroy(state->output); }
    if (state->shm)         { wl_shm_destroy(state->shm); }
    if (state->compositor)  { wl_compositor_destroy(state->compositor); }
    if (state->registry)    { wl_registry_destroy(state->registry); }
    if (state->display)     { wl_display_disconnect(state->display); }
}
