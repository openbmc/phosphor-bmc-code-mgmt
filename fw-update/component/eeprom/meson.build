
eeprom_src = files(
    'eeprom_device.cpp',
    'eeprom_device_code_updater.cpp'
)

eeprom_include = include_directories('.')

executable(
    'phosphor-fw-update-eeprom',
    'main.cpp',
    eeprom_src,
    include_directories: [
      common_include,
      eeprom_include,
    ],
    dependencies: [
      sdbusplus_dep,
      phosphor_logging_dep,
      pdi_dep,
      boost_dep,
      libgpiod,
    ],
    link_with: [libpldmutil, libfwupdate],
    install: true
)
