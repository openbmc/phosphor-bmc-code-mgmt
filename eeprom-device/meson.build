eeprom_device_version_src = files(
    'eeprom_device_version.cpp',
    'pt5161l/pt5161l.cpp',
)

eeprom_device_include = include_directories('.')

executable(
    'phosphor-eepromdevice-software-update',
    'eeprom_device_software_manager.cpp',
    'eeprom_device.cpp',
    eeprom_device_version_src,
    include_directories: [common_include, eeprom_device_include],
    dependencies: [sdbusplus_dep, phosphor_logging_dep, libgpiod_dep],
    link_with: [software_common_lib, libpldmutil],
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
)

install_data(
    'xyz.openbmc_project.Software.EEPROMDevice.service',
    install_dir: systemd_system_unit_dir,
)
