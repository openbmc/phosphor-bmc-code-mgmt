libocp_fw_recovery_inc = include_directories('include')

libocp_fw_recovery = static_library(
    'ocp_fw_recovery',
    'src/ocp_i2c.c',
    'src/ocp_recovery.c',
    include_directories: libocp_fw_recovery_inc,
    install: false,
)

libocp_fw_recovery_dep = declare_dependency(
    link_with: libocp_fw_recovery,
    include_directories: libocp_fw_recovery_inc,
)

if get_option('ocp-recovery-tool').allowed()
    subdir('tools')
endif
