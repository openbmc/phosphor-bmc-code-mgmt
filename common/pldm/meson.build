
libpldmutil = static_library(
    'pldmpackageutil',
    'package_parser.cpp',
    'pldm_package_util.cpp',
    include_directories: ['.'],
    dependencies: [pdi_dep, phosphor_logging_dep, sdbusplus_dep, libpldm_dep],
    install: false,
)

# Export dependency for other components to use
pldm_package_dep = declare_dependency(
    link_with: libpldmutil,
    include_directories: ['.'],
    dependencies: [libpldm_dep],
)
