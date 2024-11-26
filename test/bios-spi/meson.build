e = executable(
    'test_bios_spi',
    'test_bios_spi.cpp',
    bios_spi_src,
    include_directories: [
      common_include,
      bios_spi_include,
    ],
    dependencies: [
      libgpiod,
      libpldm_dep,
      sdbusplus_dep,
      pdi_dep,
      phosphor_logging_dep,
    ],
    link_with: [libpldmutil, software_common_lib],
)

test(
        'test_bios_spi',
        e,
        workdir: meson.current_source_dir(),
)
