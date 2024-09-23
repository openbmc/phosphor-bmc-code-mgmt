e = executable(
    'test_bios_spi',
    'test_bios_spi.cpp',
    bios_spi_src,
    common_src,
    include_directories: [
      common_include,
      bios_spi_include,
    ],
    dependencies: [
      common_deps,
      gpiodcxx,
      #gtest,
    ]
)

test(
        'test_bios_spi',
        e,
        workdir: meson.current_source_dir(),
)
