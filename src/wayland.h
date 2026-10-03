#ifndef WLEYES_WAYLAND_H
#define WLEYES_WAYLAND_H

#include <stdbool.h>
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <sys/types.h>

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
    bool blink;   /* --blink: blink on mouse button press */
    bool roll;    /* --roll: roll eyes on mouse wheel */
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
    struct xdg_wm_base   *wm_base;
    struct wl_surface    *pointer_focus;   /* surface under the pointer */

    /* Eyes surface */
    struct wl_surface              *eyes_surface;
    struct zwlr_layer_surface_v1   *eyes_layer;

    /* Calibration surface (fullscreen, temporary) */
    struct wl_surface              *cal_surface;
    struct zwlr_layer_surface_v1   *cal_layer;
    struct wl_buffer               *cal_buffer;
    int screen_width;
    int screen_height;

    /* Screen position of eyes surface (computed from anchor/margin) */
    double eyes_screen_x;
    double eyes_screen_y;

    /* Cursor state */
    double cursor_x;
    double cursor_y;
    bool   needs_redraw;
    bool   calibrated;

    /* Animations (CLOCK_MONOTONIC ms, 0 = inactive) */
    long   now_ms;
    long   blink_start;
    long   roll_start;
    int    roll_dir;    /* +1 clockwise, -1 counter-clockwise */

    /* libinput */
    struct libinput *li;
    int li_fd;

    /* SHM double-buffer */
    struct wl_buffer *buffers[2];
    void             *buffer_data[2];
    int               current_buffer;
    int               buffer_size;
    int               buf_width;
    int               buf_height;

    /* Right-click popup (options menu or info card) */
    struct wl_surface   *menu_surface;
    struct xdg_surface  *menu_xdg;
    struct xdg_popup    *menu_popup;
    struct wl_buffer    *menu_buffer;
    void                *menu_data;
    int                  menu_size;
    int                  menu_w, menu_h;
    int                  menu_kind;     /* enum menu_kind in menu.h */
    int                  menu_hover;    /* item index under pointer, -1 = none */

    /* Config */
    struct wleyes_config config;

    /* Run state */
    bool running;
};

bool wleyes_init(struct wleyes_state *state);
void wleyes_destroy(struct wleyes_state *state);
bool wleyes_setup_surface(struct wleyes_state *state);
bool wleyes_create_buffers(struct wleyes_state *state);
bool wleyes_setup_calibration_surface(struct wleyes_state *state);
void wleyes_destroy_calibration_surface(struct wleyes_state *state);
struct libinput;
bool wleyes_open_libinput(struct wleyes_state *state);
int  wleyes_create_shm_file(off_t size);
void wleyes_process_libinput(struct wleyes_state *state);

#endif
