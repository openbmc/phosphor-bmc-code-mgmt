if cpp.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency('CLI11')
endif

sdeventplus_dep = dependency('sdeventplus')

source = [
    'usb_manager_main.cpp',
    'usb_manager.cpp',
    '../utils.cpp',
    ]

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
