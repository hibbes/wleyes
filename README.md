# wleyes

Wayland-native xeyes — two eyes that follow your mouse cursor.

Works on any compositor with wlr-layer-shell (labwc, Sway, Hyprland, river, dwl, wayfire, COSMIC).

## Building

Dependencies: wayland-client, cairo, libinput, libudev, meson, ninja, wayland-scanner

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

1. **libinput** tracks relative cursor movement with proper acceleration
2. A temporary fullscreen transparent overlay captures the exact cursor position on first mouse movement (calibration)
3. The eyes surface provides ongoing recalibration whenever the cursor passes over it
4. With `--blink`/`--roll`, button and wheel events from the same libinput context start short time-based animations (blink 180 ms, roll 700 ms)
5. **Cairo** renders classic xeyes (white sclera, black pupils) on a wlr-layer-shell overlay

### Requirements

- User must be in the `input` group (for libinput access to `/dev/input/`)
- Compositor must support `wlr-layer-shell-unstable-v1`

## License

MIT
