i2cvr_src = files('i2cvr_device.cpp', 'i2cvr_software_manager.cpp', 'vr.cpp')

regulators_src = files('xdpe1x2xx/xdpe1x2xx.cpp')

i2cvr_include = include_directories('.')

executable(
    'phosphor-i2cvr-software-update',
    'main.cpp',
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
    install: true
)
