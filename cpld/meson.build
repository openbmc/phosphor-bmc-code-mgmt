libcpld = static_library(
    'cpld',
    'cpld_fw_handler.cpp',
    'lattice.cpp',
    include_directories: ['.', common_include],
    dependencies: [pdi_dep, phosphor_logging_dep, sdbusplus_dep, libpldm_dep],
    link_with: [software_common_lib],
)

executable(
    'cpld-code-updater',
    'main.cpp',
    'cpld_code_updater.cpp',
    include_directories: ['.', common_include],
    dependencies: [pdi_dep, phosphor_logging_dep, sdbusplus_dep, libpldm_dep],
    link_with: [libpldmutil, software_common_lib, libcpld],
)