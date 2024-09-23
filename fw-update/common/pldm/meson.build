
libpldmutil = static_library('pldmutil',
  'utils.cpp',
  'package_parser.cpp',
  'pldm_util.cpp',
  include_directories: ['.'],
  dependencies: [
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    libpldm_dep,
  ],
  install: true,
)
