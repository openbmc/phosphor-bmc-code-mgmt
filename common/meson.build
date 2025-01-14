
subdir('pldm')

software_common_lib = static_library('fwupdate',
  'src/software_manager.cpp',
  'src/device.cpp',
  'src/software_config.cpp',
  'src/software.cpp',
  'src/software_update.cpp',
  include_directories: ['.','include/', common_include],
  dependencies: [
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    libpldm_dep,
  ],
  install: false,
)
