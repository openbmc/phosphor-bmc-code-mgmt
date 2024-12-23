test(
    'test_eeprom_device',
    executable(
        'test_eeprom_device',
        'test_eeprom_device.cpp',
        eeprom_device_src,
        include_directories: [
        common_include,
        eeprom_device_include,
        ],
        dependencies: [
        sdbusplus_dep,
        phosphor_logging_dep,
        libgpiod,
        ],
        link_with: [software_common_lib, libpldmutil],
    ),
    workdir: meson.current_source_dir(),
)
