# wleyes

Wayland-native xeyes — two eyes that follow your mouse cursor.

Works on any wlroots-based compositor (labwc, Sway, Hyprland, river, dwl).

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
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `--anchor <pos>` | `top-right` | `top-left`, `top-right`, `bottom-left`, `bottom-right` |
| `--margin <x>,<y>` | `200,4` | Offset from anchor in pixels |
| `--size <w>x<h>` | `48x24` | Widget size |
| `--output <name>` | first | Target monitor |

## Install

```bash
sudo ninja -C build install
```

## How it works

1. **libinput** tracks relative cursor movement with proper acceleration
2. A temporary fullscreen transparent overlay captures the exact cursor position on first mouse movement (calibration)
3. The eyes surface provides ongoing recalibration whenever the cursor passes over it
4. **Cairo** renders classic xeyes (white sclera, black pupils) on a wlr-layer-shell overlay

### Requirements

- User must be in the `input` group (for libinput access to `/dev/input/`)
- Compositor must support `wlr-layer-shell-unstable-v1`

## License

MIT
