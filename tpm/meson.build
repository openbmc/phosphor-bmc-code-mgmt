tpm_device_include = include_directories('.')

executable(
    'phosphor-tpm-software-update',
    'tpm_software_manager.cpp',
    'tpm_device.cpp',
    'tpm2/tpm2.cpp',
    include_directories: [common_include, tpm_device_include],
    dependencies: [libpldm_dep, phosphor_logging_dep, sdbusplus_dep],
    link_with: [software_common_lib, libpldmutil],
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
)

install_data(
    'xyz.openbmc_project.Software.TPM.service',
    install_dir: systemd_system_unit_dir,
)
