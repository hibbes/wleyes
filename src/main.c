#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <wayland-client.h>

#include "wayland.h"
#include "render.h"
#include "menu.h"

#define FRAME_INTERVAL_MS 32

static struct wleyes_state *g_state = NULL;

static void handle_signal(int sig) {
    (void)sig;
    if (g_state) g_state->running = false;
}

static void print_help(const char *prog) {
    printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Wayland-native xeyes — two eyes that follow your cursor.\n"
        "\n"
        "Options:\n"
        "  --anchor <pos>      top-left|top-right|bottom-left|bottom-right (default: top-right)\n"
        "  --margin <x>,<y>    Offset from anchor in pixels (default: 200,4)\n"
        "  --size <w>x<h>      Widget size in pixels (default: 48x24)\n"
        "  --output <name>     Target monitor (default: first available)\n"
        "  --blink             Blink on every mouse button press\n"
        "  --roll              Roll the eyes when the mouse wheel turns\n"
        "  -h, --help          Show this help\n"
        "  -v, --version       Show version\n"
        "\n"
        "Right-click the eyes for a menu to toggle blink/roll and show info.\n"
        "Choices made there are saved to ~/.config/wleyes/options and override\n"
        "the command line on the next start.\n",
        prog
    );
}

static int parse_anchor(const char *s) {
    if (strcmp(s, "top-left") == 0) return ANCHOR_TOP_LEFT;
    if (strcmp(s, "top-right") == 0) return ANCHOR_TOP_RIGHT;
    if (strcmp(s, "bottom-left") == 0) return ANCHOR_BOTTOM_LEFT;
    if (strcmp(s, "bottom-right") == 0) return ANCHOR_BOTTOM_RIGHT;
    fprintf(stderr, "Invalid anchor: %s\n", s);
    exit(1);
}

static struct wleyes_config parse_args(int argc, char *argv[]) {
    struct wleyes_config cfg = {
        .anchor      = ANCHOR_TOP_RIGHT,
        .margin_x    = 200,
        .margin_y    = 4,
        .width       = 48,
        .height      = 24,
        .output_name = NULL,
        .blink       = false,
        .roll        = false,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("wleyes " WLEYES_VERSION "\n");
            exit(0);
        } else if (strcmp(argv[i], "--anchor") == 0 && i + 1 < argc) {
            cfg.anchor = parse_anchor(argv[++i]);
        } else if (strcmp(argv[i], "--margin") == 0 && i + 1 < argc) {
            if (sscanf(argv[++i], "%d,%d", &cfg.margin_x, &cfg.margin_y) != 2) {
                fprintf(stderr, "Invalid margin format, expected: x,y\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &cfg.width, &cfg.height) != 2) {
                fprintf(stderr, "Invalid size format, expected: WxH\n");
                exit(1);
            }
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            cfg.output_name = argv[++i];
        } else if (strcmp(argv[i], "--blink") == 0) {
            cfg.blink = true;
        } else if (strcmp(argv[i], "--roll") == 0) {
            cfg.roll = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            exit(1);
        }
    }
    return cfg;
}

static long time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int main(int argc, char *argv[]) {
    struct wleyes_state state = {0};
    state.config = parse_args(argc, argv);
    wleyes_options_load(&state.config);

    g_state = &state;
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    if (!wleyes_init(&state)) return 1;

    if (!wleyes_setup_surface(&state)) {
        fprintf(stderr, "wleyes: failed to create surface\n");
        wleyes_destroy(&state);
        return 1;
    }

    if (!wleyes_create_buffers(&state)) {
        fprintf(stderr, "wleyes: failed to create buffers\n");
        wleyes_destroy(&state);
        return 1;
    }

    if (!wleyes_open_libinput(&state)) {
        fprintf(stderr, "wleyes: failed to open libinput (need input group?)\n");
        wleyes_destroy(&state);
        return 1;
    }

    state.calibrated = false;
    state.cursor_x = state.eyes_screen_x + state.config.width / 2.0;
    state.cursor_y = state.eyes_screen_y + state.config.height / 2.0;

    /* Preferred: the compositor tells us the exact cursor position.
     * Fallback: libinput deltas, calibrated by a fullscreen surface that
     * catches the first mouse movement. */
    if (wleyes_setup_cursor_session(&state)) {
        state.calibrated = true;
    } else {
        wleyes_setup_calibration_surface(&state);
    }

    /* Initial render (pupils centered — cursor is at eye center) */
    wleyes_render(&state);
    wl_surface_attach(state.eyes_surface,
        state.buffers[state.current_buffer], 0, 0);
    wl_surface_damage_buffer(state.eyes_surface, 0, 0,
        state.config.width, state.config.height);
    wl_surface_commit(state.eyes_surface);
    state.current_buffer = 1 - state.current_buffer;

    int wl_fd = wl_display_get_fd(state.display);
    struct pollfd fds[2] = {
        { .fd = wl_fd,        .events = POLLIN },
        { .fd = state.li_fd,  .events = POLLIN },
    };

    long last_render = time_ms();

    while (state.running) {
        wl_display_flush(state.display);
        poll(fds, 2, 5);

        state.now_ms = time_ms();
        wleyes_process_libinput(&state);

        if (fds[0].revents & POLLIN) {
            if (wl_display_dispatch(state.display) < 0) break;
        } else {
            wl_display_dispatch_pending(state.display);
        }

        /* Calibration done → destroy fullscreen surface, clicks pass through again */
        if (state.calibrated && state.cal_surface) {
            wleyes_destroy_calibration_surface(&state);
        }

        long now = time_ms();
        state.now_ms = now;
        /* Keep drawing frames while an animation runs, plus one final frame */
        bool was_animating = state.blink_start || state.roll_start;
        if (wleyes_animating(&state) || was_animating)
            state.needs_redraw = true;
        if (state.needs_redraw && (now - last_render) >= FRAME_INTERVAL_MS) {
            wleyes_render(&state);
            wl_surface_attach(state.eyes_surface,
                state.buffers[state.current_buffer], 0, 0);
            wl_surface_damage_buffer(state.eyes_surface, 0, 0,
                state.config.width, state.config.height);
            wl_surface_commit(state.eyes_surface);
            state.current_buffer = 1 - state.current_buffer;
            state.needs_redraw = false;
            last_render = now;
        }
    }

    wleyes_destroy(&state);
    return 0;
}
