i2cvr_src = files('i2cvr_device.cpp', 'i2cvr_software_manager.cpp', 'vr.cpp')

regulators_src = files('dummyDevice/dummyDevice.cpp', 'xdpe1x2xx/xdpe1x2xx.cpp')

i2cvr_include = include_directories('.')

executable(
    'phosphor-i2cvr-software-update',
    'i2cvr_software_manager.cpp',
    i2cvr_src,
    regulators_src,
    include_directories: [common_include, i2cvr_include],
    dependencies: [
        sdbusplus_dep,
        phosphor_logging_dep,
        pdi_dep,
        boost_dep,
        libpldm_dep,
    ],
    link_with: [software_common_lib, libpldmutil],
    install: true,
)

systemd_system_unit_dir = dependency('systemd').get_variable(
    'systemdsystemunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')],
)

install_data(
    'xyz.openbmc_project.Software.I2CVR.service',
    install_dir: systemd_system_unit_dir,
)
