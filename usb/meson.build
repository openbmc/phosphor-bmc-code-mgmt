if cpp.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency(
        'CLI11',
        fallback: [ 'CLI11', 'CLI11_dep' ],
    )
endif

source = [
    'usb_manager_main.cpp',
    'usb_manager.cpp',
    '../utils.cpp',
    ]

sdeventplus_dep = dependency(
    'sdeventplus',
    fallback: [
        'sdeventplus',
        'sdeventplus_dep'
    ],
)

phosphor_logging_dep = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

executable(
    'phosphor-usb-code-update',
    source,
    include_directories: ['..'],
    dependencies: [
        CLI11_dep,
        phosphor_logging_dep,
        sdbusplus_dep,
        sdeventplus_dep,
    ],
    install: true,
    install_dir: get_option('bindir')
)

systemd_system_unit_dir = dependency('systemd').get_variable(
    pkgconfig: 'systemdsystemunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')])
udev_dir = dependency('udev').get_variable(
    pkgconfig: 'udev_dir',
    pkgconfig_define: ['prefix', get_option('prefix')])

install_data(
    'services/usb-code-update@.service',
    install_dir: systemd_system_unit_dir
)

install_data(
    '70-bmc-usb.rules',
    install_dir: udev_dir / 'rules.d'
)
