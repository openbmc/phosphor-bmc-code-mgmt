
deps = [
    cereal_dep,
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    boost_dep,
    systemd,
]

executable(
    'phosphor-fw-update-bios',
    'main.cpp',
    'bios_sw.cpp',
    'my_sw_update.cpp',
    'spi_device_code_updater.cpp',
    '../../common/code_updater.cpp',
    '../../common/software.cpp',
    include_directories: ['../../..'],
    dependencies: deps,
    install: true
)
