// src/main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "wayland.h"
#include "render.h"

#define VERSION "0.1.0"

static struct wleyes_state *g_state = NULL;

static void handle_signal(int sig) {
    (void)sig;
    if (g_state) g_state->running = false;
}

static void print_help(const char *prog) {
    printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  --anchor <pos>      Anchor position: top-left|top-right|bottom-left|bottom-right\n"
        "                      (default: top-right)\n"
        "  --margin <x>,<y>    Margin from anchor corner (default: 200,4)\n"
        "  --size <w>x<h>      Eyes surface size (default: 48x24)\n"
        "  --output <name>     Output/monitor name (default: first available)\n"
        "  -h, --help          Show this help and exit\n"
        "  -v, --version       Show version and exit\n",
        prog
    );
}

static int parse_anchor(const char *s) {
    if (strcmp(s, "top-left")     == 0) return ANCHOR_TOP_LEFT;
    if (strcmp(s, "top-right")    == 0) return ANCHOR_TOP_RIGHT;
    if (strcmp(s, "bottom-left")  == 0) return ANCHOR_BOTTOM_LEFT;
    if (strcmp(s, "bottom-right") == 0) return ANCHOR_BOTTOM_RIGHT;
    fprintf(stderr, "Unknown anchor value: %s\n", s);
    exit(EXIT_FAILURE);
}

static struct wleyes_config parse_args(int argc, char *argv[]) {
    struct wleyes_config cfg = {
        .anchor      = ANCHOR_TOP_RIGHT,
        .margin_x    = 200,
        .margin_y    = 4,
        .width       = 48,
        .height      = 24,
        .output_name = NULL,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            exit(EXIT_SUCCESS);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("wleyes " VERSION "\n");
            exit(EXIT_SUCCESS);
        } else if (strcmp(argv[i], "--anchor") == 0) {
            if (++i >= argc) { fprintf(stderr, "--anchor requires an argument\n"); exit(EXIT_FAILURE); }
            cfg.anchor = parse_anchor(argv[i]);
        } else if (strcmp(argv[i], "--margin") == 0) {
            if (++i >= argc) { fprintf(stderr, "--margin requires an argument\n"); exit(EXIT_FAILURE); }
            if (sscanf(argv[i], "%d,%d", &cfg.margin_x, &cfg.margin_y) != 2) {
                fprintf(stderr, "--margin expects format x,y (e.g. 200,4)\n");
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--size") == 0) {
            if (++i >= argc) { fprintf(stderr, "--size requires an argument\n"); exit(EXIT_FAILURE); }
            if (sscanf(argv[i], "%dx%d", &cfg.width, &cfg.height) != 2) {
                fprintf(stderr, "--size expects format WxH (e.g. 48x24)\n");
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "--output") == 0) {
            if (++i >= argc) { fprintf(stderr, "--output requires an argument\n"); exit(EXIT_FAILURE); }
            cfg.output_name = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    return cfg;
}

static const char *anchor_name(int anchor) {
    switch (anchor) {
        case ANCHOR_TOP_LEFT:     return "top-left";
        case ANCHOR_TOP_RIGHT:    return "top-right";
        case ANCHOR_BOTTOM_LEFT:  return "bottom-left";
        case ANCHOR_BOTTOM_RIGHT: return "bottom-right";
        default:                  return "unknown";
    }
}

int main(int argc, char *argv[]) {
    struct wleyes_config cfg = parse_args(argc, argv);

    printf("Config:\n");
    printf("  anchor:  %s\n",    anchor_name(cfg.anchor));
    printf("  margin:  %d,%d\n", cfg.margin_x, cfg.margin_y);
    printf("  size:    %dx%d\n", cfg.width, cfg.height);
    printf("  output:  %s\n",    cfg.output_name ? cfg.output_name : "(first available)");

    struct wleyes_state state = {0};
    state.config = cfg;

    g_state = &state;
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    if (!wleyes_init(&state)) {
        fprintf(stderr, "Failed to connect to Wayland.\n");
        return EXIT_FAILURE;
    }

    printf("Connected to Wayland. Layer shell: %s\n",
           state.layer_shell ? "yes" : "no");

    if (!wleyes_setup_surfaces(&state)) {
        fprintf(stderr, "Failed to set up surfaces.\n");
        wleyes_destroy(&state);
        return EXIT_FAILURE;
    }

    /* Process configure events from compositor */
    wl_display_roundtrip(state.display);

    printf("Screen: %dx%d\n", state.screen_width, state.screen_height);

    if (!wleyes_create_buffers(&state)) {
        fprintf(stderr, "Failed to create SHM buffers.\n");
        wleyes_destroy(&state);
        return EXIT_FAILURE;
    }

    printf("Tracking pointer. Press Ctrl+C to quit.\n");

    while (state.running && wl_display_dispatch(state.display) != -1) {
        if (state.needs_redraw && state.cursor_valid) {
            wleyes_render(&state);

            wl_surface_attach(state.eyes_surface,
                state.buffers[state.current_buffer], 0, 0);
            wl_surface_damage_buffer(state.eyes_surface, 0, 0,
                state.config.width, state.config.height);
            wl_surface_commit(state.eyes_surface);

            state.current_buffer = 1 - state.current_buffer;
            state.needs_redraw = false;
        }
    }

    printf("\nShutting down.\n");

    wleyes_destroy(&state);
    return EXIT_SUCCESS;
}
