
subdir('pldm')

libfwupdate = static_library('fwupdate',
  'src/fw_manager.cpp',
  'src/device.cpp',
  'src/software.cpp',
  'src/software-update.cpp',
  include_directories: ['.','include/', common_include],
  dependencies: [
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    libpldm_dep,
  ],
  install: true,
)

