
bios_spi_src = files('bios_software_manager.cpp'
, 'spi_device.cpp')

bios_spi_include = include_directories('.')

executable(
    'phosphor-bios-software-update',
    'main.cpp',
    bios_spi_src,
    include_directories: [common_include, bios_spi_include],
    dependencies: [
        sdbusplus_dep,
        phosphor_logging_dep,
        pdi_dep,
        boost_dep,
        libgpiod,
        libpldm_dep,
    ],
    link_with: [libpldmutil, software_common_lib],
    install: true,
)

systemd_system_unit_dir = dependency('systemd').get_variable(
    'systemdsystemunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')],
)

install_data(
    'xyz.openbmc_project.Software.BIOS.service',
    install_dir: systemd_system_unit_dir,
)
