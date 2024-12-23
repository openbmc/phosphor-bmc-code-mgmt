
eeprom_device_src = files(
    'eeprom_device.cpp',
    'eeprom_device_code_updater.cpp'
)

eeprom_device_include = include_directories('.')

libgpiod = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: '>=1.1.2'
)

executable(
    'phosphor-fw-update-eeprom-device',
    'main.cpp',
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
    install: true
)
