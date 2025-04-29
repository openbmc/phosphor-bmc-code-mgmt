
subdir('pldm')

if get_option('i2cvr-software-update').enabled()
    libi2c_dev = static_library('i2c_dev', 'i2c/i2c.cpp', link_args: '-li2c')
    libi2c_inc = include_directories('include/i2c/')
    libi2c_dep = declare_dependency(
        link_with: libi2c_dev,
        include_directories: libi2c_inc,
        link_args: '-li2c',
    )
endif

software_common_lib = static_library(
    'software_common_lib',
    'src/software_manager.cpp',
    'src/device.cpp',
    'src/software_config.cpp',
    'src/software.cpp',
    'src/software_update.cpp',
    'src/host_power.cpp',
    include_directories: ['.', 'include/', common_include],
    dependencies: [pdi_dep, phosphor_logging_dep, sdbusplus_dep, libpldm_dep],
)
