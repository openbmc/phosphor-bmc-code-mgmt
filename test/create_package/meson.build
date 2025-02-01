
libpldmcreatepkg = static_library(
    'pldmcreatepkg',
    'create_pldm_fw_package.cpp',
    'firmware_device_id_area.cpp',
    'component_image_info_area.cpp',
    include_directories: ['.'],
    dependencies: [phosphor_logging_dep, libpldm_dep],
    install: false,
)
