project('wl-gammactl', 'c')
dep_gtk3 = dependency('gtk+-3.0')
dep_wlroots = dependency('wlroots')
dep_wayland_client = dependency('wayland-client')
sources = ['src/main.c', 'src/wlr-gamma-control-unstable-v1-client-protocol.c', 'src/wlr-gamma-control-unstable-v1-client-protocol.h', 'src/resources.c']
executable('wl-gammactl', sources, dependencies : [dep_wlroots, dep_wayland_client, dep_gtk3], link_args: ['-rdynamic', '-lm'])
