source = ['side_switch.cpp', '../utils.cpp']

executable(
    'phosphor-bmc-side-switch',
    source,
    include_directories: ['..'],
    dependencies: [phosphor_logging_dep],
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
)

install_data(
    'phosphor-bmc-side-switch.service',
    install_dir: systemd_system_unit_dir,
)
