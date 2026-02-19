
libpldmutil = static_library(
    'pldmpackageutil',
    'pldm_package_util.cpp',
    include_directories: ['.'],
    dependencies: [
        pdi_dep,
        phosphor_logging_dep,
        sdbusplus_dep,
        libpldm_dep,
        libpldmcpp_dep,
    ],
    install: false,
)
