project('wleyes', 'c',
  version: '0.3.0',
  default_options: ['c_std=c11', 'warning_level=2'],
)

add_project_arguments('-DWLEYES_VERSION="@0@"'.format(meson.project_version()), language: 'c')

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

# Exact cursor position from the compositor (ext-image-copy-capture cursor
# session). Optional at runtime: used when the compositor offers it.
staging = wayland_protos.get_variable('pkgdatadir') / 'staging'
extra_protocols = []
foreach proto : [
    # referenced by ext-image-capture-source (toplevel sources), not used directly
    ['ext-foreign-toplevel-list', staging / 'ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml'],
    ['ext-image-capture-source', staging / 'ext-image-capture-source/ext-image-capture-source-v1.xml'],
    ['ext-image-copy-capture',   staging / 'ext-image-copy-capture/ext-image-copy-capture-v1.xml'],
  ]
  extra_protocols += custom_target(proto[0] + '-client-header',
    input: proto[1],
    output: proto[0] + '-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  extra_protocols += custom_target(proto[0] + '-code',
    input: proto[1],
    output: proto[0] + '-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
endforeach

executable('wleyes',
  'src/main.c',
  'src/wayland.c',
  'src/render.c',
  'src/menu.c',
  layer_shell_client_h,
  layer_shell_code,
  xdg_shell_client_h,
  xdg_shell_code,
  extra_protocols,
  dependencies: [wayland_client, cairo, math_lib, rt_lib, libinput, libudev],
  install: true,
)
