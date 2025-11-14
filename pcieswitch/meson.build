
pcieswitch_spi_src = files('pcieswitch_software_manager.cpp'
, 'spi_device.cpp')

pcieswitch_spi_include = include_directories('.')

executable(
    'phosphor-pcieswitch-software-update',
    pcieswitch_spi_src,
    include_directories: [common_include, pcieswitch_spi_include],
    dependencies: [
        sdbusplus_dep,
        phosphor_logging_dep,
        pdi_dep,
        boost_dep,
        libgpiod_dep,
        libpldm_dep,
    ],
    link_with: [libpldmutil, software_common_lib],
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
)

install_data(
    'xyz.openbmc_project.Software.PCIESWITCH.service',
    install_dir: systemd_system_unit_dir,
)
