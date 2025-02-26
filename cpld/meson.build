cpld_src = files(
    'cpld_code_updater.cpp',
    'cpld_interface.cpp',
    'cpld.cpp',
)

cpld_vendor_src = files(
    'lattice/interface.cpp',
    'lattice/lattice.cpp',
    'lattice/cpld_fw_handler.cpp',
)

executable(
    'cpld-code-updater',
    'main.cpp',
    cpld_src,
    cpld_vendor_src,
    include_directories: [include_directories('.'), common_include],
    dependencies: [pdi_dep, phosphor_logging_dep, sdbusplus_dep, libpldm_dep],
    link_with: [libpldmutil, software_common_lib],
    install: true,
)
