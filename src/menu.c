#define _GNU_SOURCE
#include "menu.h"
#include "wayland.h"

#include <cairo/cairo.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef WLEYES_VERSION
#define WLEYES_VERSION "dev"
#endif

/* ── layout ──────────────────────────────────────────────────────────────── */

#define MENU_W      250
#define INFO_W      280
#define PAD         6
#define ITEM_H      28
#define SEP_H       9
#define RADIUS      8.0
#define FONT_SIZE   13.0

enum item_type { ITEM_CHECK_BLINK, ITEM_CHECK_ROLL, ITEM_SEPARATOR, ITEM_INFO };

static const enum item_type options_items[] = {
    ITEM_CHECK_BLINK, ITEM_CHECK_ROLL, ITEM_SEPARATOR, ITEM_INFO,
};
#define N_OPTIONS (int)(sizeof(options_items) / sizeof(options_items[0]))

/* ── strings (German if the locale says so, English otherwise) ───────────── */

struct strings {
    const char *blink, *roll, *info;
    const char *tagline, *license;
};

static const struct strings str_en = {
    "Blink on click", "Roll eyes on scroll", "Info",
    "Wayland-native xeyes", "MIT License",
};
static const struct strings str_de = {
    "Blinzeln bei Klick", "Augen rollen am Mausrad", "Info",
    "xeyes für Wayland", "MIT-Lizenz",
};

static const struct strings *strings(void) {
    const char *vars[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
    for (size_t i = 0; i < 3; i++) {
        const char *v = getenv(vars[i]);
        if (v && *v) return strncmp(v, "de", 2) == 0 ? &str_de : &str_en;
    }
    return &str_en;
}

/* ── colours (neutral dark, readable on light and dark panels) ───────────── */

static void set_bg(cairo_t *cr)     { cairo_set_source_rgb(cr, 0.16, 0.16, 0.18); }
static void set_border(cairo_t *cr) { cairo_set_source_rgb(cr, 0.32, 0.33, 0.38); }
static void set_hover(cairo_t *cr)  { cairo_set_source_rgb(cr, 0.26, 0.27, 0.31); }
static void set_text(cairo_t *cr)   { cairo_set_source_rgb(cr, 0.92, 0.92, 0.94); }
static void set_dim(cairo_t *cr)    { cairo_set_source_rgb(cr, 0.65, 0.66, 0.70); }
static void set_accent(cairo_t *cr) { cairo_set_source_rgb(cr, 0.54, 0.71, 0.98); }

static void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

/* ── geometry ────────────────────────────────────────────────────────────── */

static int item_height(enum item_type t) { return t == ITEM_SEPARATOR ? SEP_H : ITEM_H; }

static void popup_size(int kind, int *w, int *h) {
    if (kind == MENU_INFO) {
        *w = INFO_W;
        *h = 2 * PAD + 6 + 24 + 4 * 20 + 6;
        return;
    }
    int y = 2 * PAD;
    for (int i = 0; i < N_OPTIONS; i++) y += item_height(options_items[i]);
    *w = MENU_W;
    *h = y;
}

static int item_at(double y) {
    if (y < PAD) return -1;
    double top = PAD;
    for (int i = 0; i < N_OPTIONS; i++) {
        double h = item_height(options_items[i]);
        if (y < top + h) return options_items[i] == ITEM_SEPARATOR ? -1 : i;
        top += h;
    }
    return -1;
}

/* ── drawing ─────────────────────────────────────────────────────────────── */

static void draw_check(cairo_t *cr, double x, double y, bool on) {
    rounded_rect(cr, x, y, 14, 14, 3);
    if (on) {
        set_accent(cr);
        cairo_fill(cr);
        cairo_move_to(cr, x + 3.2, y + 7.4);
        cairo_line_to(cr, x + 6.0, y + 10.2);
        cairo_line_to(cr, x + 11.0, y + 4.0);
        cairo_set_source_rgb(cr, 0.10, 0.10, 0.12);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
    } else {
        set_dim(cr);
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);
    }
}

static void draw_options(cairo_t *cr, const struct wleyes_state *st, const struct strings *s) {
    double y = PAD;
    for (int i = 0; i < N_OPTIONS; i++) {
        enum item_type t = options_items[i];
        double h = item_height(t);
        if (t == ITEM_SEPARATOR) {
            set_border(cr);
            cairo_rectangle(cr, PAD + 4, y + SEP_H / 2.0, st->menu_w - 2 * PAD - 8, 1);
            cairo_fill(cr);
            y += h;
            continue;
        }
        if (st->menu_hover == i) {
            set_hover(cr);
            rounded_rect(cr, PAD, y, st->menu_w - 2 * PAD, h, 5);
            cairo_fill(cr);
        }
        const char *label = t == ITEM_CHECK_BLINK ? s->blink
                          : t == ITEM_CHECK_ROLL  ? s->roll : s->info;
        double tx = PAD + 10;
        if (t == ITEM_CHECK_BLINK || t == ITEM_CHECK_ROLL) {
            bool on = t == ITEM_CHECK_BLINK ? st->config.blink : st->config.roll;
            draw_check(cr, tx, y + (h - 14) / 2, on);
        }
        tx += 24;
        set_text(cr);
        cairo_move_to(cr, tx, y + h / 2 + FONT_SIZE / 2.8);
        cairo_show_text(cr, label);
        y += h;
    }
}

static void draw_info(cairo_t *cr, const struct strings *s) {
    double x = PAD + 12, y = PAD + 6 + 18;
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, FONT_SIZE + 3);
    set_text(cr);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, "wleyes " WLEYES_VERSION);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, FONT_SIZE);
    const char *lines[] = {
        s->tagline,
        "\xC2\xA9 2026 Marek Czernohous",
        "github.com/hibbes/wleyes",
        s->license,
    };
    y += 6;
    for (int i = 0; i < 4; i++) {
        y += 20;
        if (i == 2) set_accent(cr); else if (i == 3) set_dim(cr); else set_text(cr);
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, lines[i]);
    }
}

/* Pure drawing into an ARGB32 buffer of menu_w x menu_h */
static void menu_paint(const struct wleyes_state *st, void *data) {
    cairo_surface_t *surf = cairo_image_surface_create_for_data(
        data, CAIRO_FORMAT_ARGB32, st->menu_w, st->menu_h, st->menu_w * 4);
    cairo_t *cr = cairo_create(surf);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    rounded_rect(cr, 0.5, 0.5, st->menu_w - 1, st->menu_h - 1, RADIUS);
    set_bg(cr);
    cairo_fill_preserve(cr);
    set_border(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    const struct strings *s = strings();
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, FONT_SIZE);
    if (st->menu_kind == MENU_INFO) draw_info(cr, s);
    else draw_options(cr, st, s);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

static void menu_redraw(struct wleyes_state *st) {
    if (!st->menu_data || !st->menu_surface) return;
    menu_paint(st, st->menu_data);
    wl_surface_attach(st->menu_surface, st->menu_buffer, 0, 0);
    wl_surface_damage_buffer(st->menu_surface, 0, 0, st->menu_w, st->menu_h);
    wl_surface_commit(st->menu_surface);
}

/* ── xdg listeners ───────────────────────────────────────────────────────── */

static void menu_xdg_configure(void *data, struct xdg_surface *xdg, uint32_t serial) {
    struct wleyes_state *st = data;
    xdg_surface_ack_configure(xdg, serial);
    menu_redraw(st);
}

static const struct xdg_surface_listener menu_xdg_listener = {
    .configure = menu_xdg_configure,
};

static void popup_configure(void *data, struct xdg_popup *p,
        int32_t x, int32_t y, int32_t w, int32_t h) {
    (void)data; (void)p; (void)x; (void)y; (void)w; (void)h;
}

static void popup_done(void *data, struct xdg_popup *p) {
    (void)p;
    wleyes_menu_close(data);
}

static void popup_repositioned(void *data, struct xdg_popup *p, uint32_t token) {
    (void)data; (void)p; (void)token;
}

static const struct xdg_popup_listener popup_listener = {
    .configure     = popup_configure,
    .popup_done    = popup_done,
    .repositioned  = popup_repositioned,
};

/* ── public API ──────────────────────────────────────────────────────────── */

void wleyes_menu_close(struct wleyes_state *st) {
    if (st->menu_popup)   { xdg_popup_destroy(st->menu_popup); st->menu_popup = NULL; }
    if (st->menu_xdg)     { xdg_surface_destroy(st->menu_xdg); st->menu_xdg = NULL; }
    if (st->menu_surface) {
        if (st->pointer_focus == st->menu_surface) st->pointer_focus = NULL;
        wl_surface_destroy(st->menu_surface);
        st->menu_surface = NULL;
    }
    if (st->menu_buffer)  { wl_buffer_destroy(st->menu_buffer); st->menu_buffer = NULL; }
    if (st->menu_data)    { munmap(st->menu_data, (size_t)st->menu_size); st->menu_data = NULL; }
    st->menu_kind = MENU_NONE;
    st->menu_hover = -1;
}

void wleyes_menu_open(struct wleyes_state *st, int kind, uint32_t serial) {
    wleyes_menu_close(st);
    if (!st->wm_base || !st->seat) return;

    popup_size(kind, &st->menu_w, &st->menu_h);
    st->menu_size = st->menu_w * st->menu_h * 4;
    int fd = wleyes_create_shm_file(st->menu_size);
    if (fd < 0) return;
    st->menu_data = mmap(NULL, (size_t)st->menu_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (st->menu_data == MAP_FAILED) { st->menu_data = NULL; close(fd); return; }
    struct wl_shm_pool *pool = wl_shm_create_pool(st->shm, fd, st->menu_size);
    st->menu_buffer = wl_shm_pool_create_buffer(pool, 0, st->menu_w, st->menu_h,
                                                st->menu_w * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    st->menu_kind = kind;
    st->menu_hover = -1;
    st->menu_surface = wl_compositor_create_surface(st->compositor);
    st->menu_xdg = xdg_wm_base_get_xdg_surface(st->wm_base, st->menu_surface);
    xdg_surface_add_listener(st->menu_xdg, &menu_xdg_listener, st);

    /* Below the eyes when anchored at the top, above them at the bottom;
     * the compositor slides/flips it to stay on screen. */
    bool top = st->config.anchor == ANCHOR_TOP_LEFT || st->config.anchor == ANCHOR_TOP_RIGHT;
    struct xdg_positioner *pos = xdg_wm_base_create_positioner(st->wm_base);
    xdg_positioner_set_size(pos, st->menu_w, st->menu_h);
    xdg_positioner_set_anchor_rect(pos, 0, 0, st->config.width, st->config.height);
    xdg_positioner_set_anchor(pos, top ? XDG_POSITIONER_ANCHOR_BOTTOM : XDG_POSITIONER_ANCHOR_TOP);
    xdg_positioner_set_gravity(pos, top ? XDG_POSITIONER_GRAVITY_BOTTOM : XDG_POSITIONER_GRAVITY_TOP);
    xdg_positioner_set_offset(pos, 0, top ? 4 : -4);
    xdg_positioner_set_constraint_adjustment(pos,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);

    st->menu_popup = xdg_surface_get_popup(st->menu_xdg, NULL, pos);
    xdg_positioner_destroy(pos);
    xdg_popup_add_listener(st->menu_popup, &popup_listener, st);
    zwlr_layer_surface_v1_get_popup(st->eyes_layer, st->menu_popup);
    /* Grab: a click anywhere else dismisses the popup (popup_done). */
    xdg_popup_grab(st->menu_popup, st->seat, serial);
    wl_surface_commit(st->menu_surface);
}

void wleyes_menu_hover(struct wleyes_state *st, double x, double y) {
    if (st->menu_kind != MENU_OPTIONS) return;
    int hover = (x < 0 || y < 0) ? -1 : item_at(y);
    if (hover != st->menu_hover) {
        st->menu_hover = hover;
        menu_redraw(st);
    }
}

void wleyes_menu_click(struct wleyes_state *st, uint32_t serial) {
    if (st->menu_kind == MENU_INFO) {
        wleyes_menu_close(st);
        return;
    }
    if (st->menu_hover < 0) return;
    switch (options_items[st->menu_hover]) {
    case ITEM_CHECK_BLINK:
        st->config.blink = !st->config.blink;
        if (!st->config.blink) st->blink_start = 0;
        wleyes_options_save(&st->config);
        menu_redraw(st);
        break;
    case ITEM_CHECK_ROLL:
        st->config.roll = !st->config.roll;
        if (!st->config.roll) st->roll_start = 0;
        wleyes_options_save(&st->config);
        menu_redraw(st);
        break;
    case ITEM_INFO:
        wleyes_menu_open(st, MENU_INFO, serial);
        break;
    case ITEM_SEPARATOR:
        break;
    }
    st->needs_redraw = true;
}

/* ── persisted options ───────────────────────────────────────────────────── */

static bool options_path(char *buf, size_t len, bool make_dir) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char dir[4096];
    if (xdg && *xdg) snprintf(dir, sizeof dir, "%s/wleyes", xdg);
    else if (home && *home) snprintf(dir, sizeof dir, "%s/.config/wleyes", home);
    else return false;
    if (make_dir && mkdir(dir, 0755) < 0 && errno != EEXIST) return false;
    snprintf(buf, len, "%s/options", dir);
    return true;
}

void wleyes_options_load(struct wleyes_config *cfg) {
    char path[4200];
    if (!options_path(path, sizeof path, false)) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        int v;
        if (sscanf(line, "blink=%d", &v) == 1) cfg->blink = v != 0;
        else if (sscanf(line, "roll=%d", &v) == 1) cfg->roll = v != 0;
    }
    fclose(f);
}

void wleyes_options_save(const struct wleyes_config *cfg) {
    char path[4200];
    if (!options_path(path, sizeof path, true)) return;
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "wleyes: cannot write %s\n", path);
        return;
    }
    fprintf(f, "# written by the wleyes right-click menu\nblink=%d\nroll=%d\n",
            cfg->blink ? 1 : 0, cfg->roll ? 1 : 0);
    fclose(f);
}
