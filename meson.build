project('wleyes', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=2'],
)

cc = meson.get_compiler('c')
math_lib = cc.find_library('m')
rt_lib = cc.find_library('rt', required: false)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
cairo = dependency('cairo')
libinput = dependency('libinput')
libudev = dependency('libudev')

wayland_scanner = find_program('wayland-scanner')

# Generate layer-shell protocol code
layer_shell_xml = files('protocols/wlr-layer-shell-unstable-v1.xml')

layer_shell_client_h = custom_target('layer-shell-client-header',
  input: layer_shell_xml,
  output: 'wlr-layer-shell-unstable-v1-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

layer_shell_code = custom_target('layer-shell-code',
  input: layer_shell_xml,
  output: 'wlr-layer-shell-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

# Also generate xdg-shell for wl_shm format negotiation
xdg_shell_xml = wayland_protos.get_variable('pkgdatadir') / 'stable/xdg-shell/xdg-shell.xml'

xdg_shell_client_h = custom_target('xdg-shell-client-header',
  input: xdg_shell_xml,
  output: 'xdg-shell-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

xdg_shell_code = custom_target('xdg-shell-code',
  input: xdg_shell_xml,
  output: 'xdg-shell-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

executable('wleyes',
  'src/main.c',
  'src/wayland.c',
  'src/render.c',
  layer_shell_client_h,
  layer_shell_code,
  xdg_shell_client_h,
  xdg_shell_code,
  dependencies: [wayland_client, cairo, math_lib, rt_lib, libinput, libudev],
  install: true,
)
