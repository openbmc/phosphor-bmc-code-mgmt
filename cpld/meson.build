cpld_src = files('cpld.cpp', 'cpld_interface.cpp', 'cpld_software_manager.cpp')

cpld_vendor_src = files('lattice/interface.cpp', 'lattice/lattice.cpp')

executable(
    'phosphor-cpld-software-update',
    cpld_src,
    cpld_vendor_src,
    include_directories: [include_directories('.'), common_include, libi2c_inc],
    dependencies: [pdi_dep, phosphor_logging_dep, sdbusplus_dep, libpldm_dep, libi2c_dep],
    link_with: [libpldmutil, software_common_lib, libi2c_dev],
    link_args: '-li2c',
    install: true,
)
