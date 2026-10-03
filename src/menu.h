#ifndef WLEYES_MENU_H
#define WLEYES_MENU_H

#include <stdint.h>

struct wleyes_state;
struct wleyes_config;

enum menu_kind {
    MENU_NONE    = 0,
    MENU_OPTIONS = 1,   /* checkable options + "Info" */
    MENU_INFO    = 2,   /* about card, any click closes it */
};

/* Open a popup next to the eyes; serial comes from the triggering click. */
void wleyes_menu_open(struct wleyes_state *state, int kind, uint32_t serial);
void wleyes_menu_close(struct wleyes_state *state);
/* Surface-local pointer position; negative = pointer left the popup. */
void wleyes_menu_hover(struct wleyes_state *state, double x, double y);
void wleyes_menu_click(struct wleyes_state *state, uint32_t serial);

/* Options chosen in the menu persist in $XDG_CONFIG_HOME/wleyes/options
 * and override the command line on the next start. */
void wleyes_options_load(struct wleyes_config *cfg);
void wleyes_options_save(const struct wleyes_config *cfg);

#endif
