i2cvr_src = files('i2cvr_device.cpp', 'i2cvr_software_manager.cpp', 'vr.cpp')

regulators_src = files('isl69269/isl69269.cpp', 'xdpe1x2xx/xdpe1x2xx.cpp')

i2cvr_include = include_directories('.')

executable(
    'phosphor-i2cvr-software-update',
    'i2cvr_software_manager.cpp',
    i2cvr_src,
    regulators_src,
    include_directories: [common_include, i2cvr_include, libi2c_inc],
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
    'xyz.openbmc_project.Software.I2CVR.service',
    install_dir: systemd_system_unit_dir,
)
