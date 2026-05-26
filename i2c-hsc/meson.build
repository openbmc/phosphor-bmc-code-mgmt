i2chsc_src = files('i2chsc_device.cpp', 'i2chsc_software_manager.cpp', 'hsc.cpp')

controller_src = files(
    'tps25990/tps25990.cpp',
    'tps25990/rs31390.cpp',
)

i2chsc_include = include_directories('.')

executable(
    'phosphor-i2chsc-software-update',
    'i2chsc_software_manager.cpp',
    i2chsc_src,
    controller_src,
    include_directories: [common_include, i2chsc_include, libi2c_inc],
    dependencies: [
        sdbusplus_dep,
        phosphor_logging_dep,
        pdi_dep,
        boost_dep,
        libpldm_dep,
        libi2c_dep,
    ],
    link_with: [software_common_lib, libpldmutil, libi2c_dev],
    install: true,
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
    link_args: '-li2c',
)

install_data(
    'xyz.openbmc_project.Software.I2CHSC.service',
    install_dir: systemd_system_unit_dir,
)
