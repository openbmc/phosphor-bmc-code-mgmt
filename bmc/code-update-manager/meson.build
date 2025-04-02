inc_directories = include_directories(['.', '../../common/include'])
executable(
    'code-update-manager',
    ['main.cpp', 'update_manager.cpp'],
    include_directories: inc_directories,
    dependencies: [phosphor_logging_dep, sdbusplus_dep],
    link_with: [software_common_lib],
    install: true,
    install_dir: get_option('bindir'),
)

# Install service file
configure_file(
    input: 'xyz.openbmc_project.Software.Update.Manager.service.in',
    output: 'xyz.openbmc_project.Software.Update.Manager.service',
    configuration: conf,
    install: true,
    install_dir: systemd_system_unit_dir,
)
