source = ['side_switch.cpp', '../utils.cpp']

executable(
    'phosphor-bmc-side-switch',
    source,
    include_directories: ['..'],
    dependencies: [phosphor_logging_dep],
    install: true,
    install_dir: get_option('bindir'),
)

systemd_system_unit_dir = dependency('systemd').get_variable(
    'systemdsystemunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')],
)

install_data(
    'phosphor-bmc-side-switch.service',
    install_dir: systemd_system_unit_dir,
)
