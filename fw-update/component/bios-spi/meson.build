
bios_spi_src = files(
    'spi_device_sw.cpp',
    'spi_device.cpp',
    'spi_device_sw_update.cpp',
    'spi_device_code_updater.cpp'
)

bios_spi_include = include_directories('.')

executable(
    'phosphor-fw-update-bios',
    'main.cpp',
    bios_spi_src,
    include_directories: [
      common_include,
      bios_spi_include,
    ],
    dependencies: [
      sdbusplus_dep,
      phosphor_logging_dep,
      pdi_dep,
      boost_dep,
      gpiodcxx,
    ],
    link_with: [libpldmutil, libfwupdate],
    install: true
)
