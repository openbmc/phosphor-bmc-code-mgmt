libexampledevice = static_library('example_device',
  'example_device.cpp',
  include_directories: ['.', common_include],
  dependencies: [
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    libpldm_dep,
  ],
  link_with: [
    software_common_lib,
  ],
  install: false,
)

executable(
  'example-code-updater',
  'main.cpp',
  include_directories: ['.', common_include],
  dependencies: [
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    libpldm_dep,
  ],
  link_with: [
    libpldmutil,
    software_common_lib,
    libexampledevice
  ],
  install: false,
)
