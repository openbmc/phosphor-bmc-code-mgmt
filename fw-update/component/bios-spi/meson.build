
executable(
    'phosphor-fw-update-bios',
    'main.cpp',
    'spi_device_sw.cpp',
    'spi_device.cpp',
    'spi_device_sw_update.cpp',
    'spi_device_code_updater.cpp',
    common_src,
    include_directories: [
      common_include,
    ],
    dependencies: [
      common_deps,
      gpiodcxx,
    ],
    install: true
)
