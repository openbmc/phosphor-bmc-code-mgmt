libnopdevice = static_library('nopdevice',
  'nopdevice.cpp',
  include_directories: ['.', common_include],
  dependencies: [
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    libpldm_dep,
  ],
  link_with: [
    libfwupdate,
  ],
  install: false,
)

