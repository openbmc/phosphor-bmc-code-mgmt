executable(
    'phosphor-ocp-recovery-software-update',
    'ocp_recovery_software_manager.cpp',
    'ocp_recovery_device.cpp',
    include_directories: [common_include],
    dependencies: [
        sdbusplus_dep,
        phosphor_logging_dep,
        pdi_dep,
        boost_dep,
        libpldm_dep,
        libocp_fw_recovery_dep,
    ],
    link_with: [software_common_lib, libpldmutil],
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
)

install_data(
    'xyz.openbmc_project.Software.OCPRecovery.service',
    install_dir: systemd_system_unit_dir,
)
