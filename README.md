# wleyes

Wayland-native xeyes — two eyes that follow your mouse cursor.

Works on any compositor with wlr-layer-shell (labwc, Sway, Hyprland, river, dwl, wayfire, COSMIC).

## Building

Dependencies: wayland-client, wayland-protocols (≥ 1.37 for the staging capture protocols), cairo, libinput, libudev, meson, ninja, wayland-scanner

```bash
meson setup build
ninja -C build
```

## Usage

```bash
./build/wleyes                                      # default: top-right corner
./build/wleyes --size 48x24 --margin 380,4          # waybar positioning
./build/wleyes --anchor top-left --margin 10,4
./build/wleyes --blink --roll                       # blink on clicks, roll on scroll
```

### Right-click menu

Right-click the eyes for a small popup menu:

- **Blink on click** / **Roll eyes on scroll**: toggle the two animations at runtime. The choice is saved to `~/.config/wleyes/options` (or `$XDG_CONFIG_HOME/wleyes/options`) and overrides `--blink`/`--roll` on the next start.
- **Info**: version, author and project link.

Clicking anywhere else closes the popup. Labels are German when the locale is German, English otherwise.

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `--anchor <pos>` | `top-right` | `top-left`, `top-right`, `bottom-left`, `bottom-right` |
| `--margin <x>,<y>` | `200,4` | Offset from anchor in pixels |
| `--size <w>x<h>` | `48x24` | Widget size |
| `--output <name>` | first | Target monitor |
| `--blink` | off | Blink on every mouse button press |
| `--roll` | off | Roll the eyes once around when the mouse wheel turns (direction follows the wheel) |

## Install

```bash
sudo ninja -C build install
```

## How it works

1. **Exact cursor (preferred):** if the compositor offers `ext-image-copy-capture-v1` with `ext-image-capture-source-v1` (COSMIC does), wleyes opens a pointer cursor session on the output and gets the real cursor position on every move. Nothing can drift.
2. **Fallback:** **libinput** tracks relative cursor movement with proper acceleration. A temporary fullscreen transparent overlay captures the exact cursor position on first mouse movement (calibration), and the eyes surface recalibrates whenever the cursor passes over it. This can drift when the compositor moves or holds the pointer itself (VT switch, pointer lock in games, warps).
3. Mouse buttons and the wheel always come from libinput, for `--blink`/`--roll` (blink 180 ms, roll 700 ms)
4. A right-click on the eyes opens an `xdg_popup` attached to the layer surface (`zwlr_layer_surface_v1.get_popup`) with a pointer grab, so a click elsewhere dismisses it
5. **Cairo** renders classic xeyes (white sclera, black pupils) on a wlr-layer-shell overlay

### Requirements

- User must be in the `input` group (for libinput access to `/dev/input/`)
- Compositor must support `wlr-layer-shell-unstable-v1`

## License

MIT
