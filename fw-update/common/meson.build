
subdir('pldm')
subdir('create_package')

libfwupdate = static_library('fwupdate',
  'fw_manager.cpp',
  'device.cpp',
  'software.cpp',
  'software-version.cpp',
  'software-activation.cpp',
  'software-update.cpp',
  'software-association-definitions.cpp',
  include_directories: ['.', common_include],
  dependencies: [
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    libpldm_dep,
  ],
  install: true,
)

