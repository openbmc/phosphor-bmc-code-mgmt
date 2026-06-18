libspi_inc = include_directories('../include/spi/')
libspi_dev = static_library(
    'spi_dev',
    'spi_device.cpp',
    dependencies: [sdbusplus_dep],
    include_directories: [common_include, libspi_inc]
)
libspi_dep = declare_dependency(
    dependencies: [sdbusplus_dep],
    include_directories: [common_include, libspi_inc]
)
