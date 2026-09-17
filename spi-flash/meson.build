spi_src = files(
    'bios/bios_device.cpp',
    'devices/bcm51358.cpp',
    'spi_device.cpp',
    'spi_factory.cpp',
    'spi_software_manager.cpp',
)

executable(
    'phosphor-spi-flash-software-update',
    spi_src,
    include_directories: [common_include, include_directories('.'), gpio_inc],
    dependencies: [
        sdbusplus_dep,
        phosphor_logging_dep,
        libgpiod_dep,
        libserial_dep,
        pdi_dep,
        boost_dep,
        libpldm_dep,
    ],
    link_with: [
      libpldmutil,
      software_common_lib,
      libgpio_controller,
      libserial_dev,
    ],
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
)

install_data(
    'xyz.openbmc_project.Software.SPIFlash.service',
    install_dir: systemd_system_unit_dir,
)
