
libpldmutil = static_library(
    'pldmpackageutil',
    'pldm_package_util.cpp',
    'firmware_parameters.cpp',
    include_directories: ['.', '../include'],
    dependencies: [
        pdi_dep,
        phosphor_logging_dep,
        sdbusplus_dep,
        libpldm_dep,
        libpldmcpp_dep,
    ],
    install: false,
)
