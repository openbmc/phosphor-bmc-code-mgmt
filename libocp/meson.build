libocp_inc = include_directories('include')

libocp = static_library(
    'ocp',
    'src/ocp_i2c.c',
    'src/ocp_recovery.c',
    include_directories: libocp_inc,
    override_options: ['c_std=c17'],
    install: false,
)

libocp_dep = declare_dependency(
    link_with: libocp,
    include_directories: libocp_inc,
)