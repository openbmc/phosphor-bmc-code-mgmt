
subdir('pldm')

software_common_lib = static_library(
    'software_common_lib',
    'src/software_manager.cpp',
    'src/device.cpp',
    'src/software_config.cpp',
    'src/software.cpp',
    'src/software_update.cpp',
    'i2c/i2c.cpp',
    include_directories: ['.', 'include/', 'include/i2c/', common_include],
    dependencies: [pdi_dep, phosphor_logging_dep, sdbusplus_dep, libpldm_dep],
)
