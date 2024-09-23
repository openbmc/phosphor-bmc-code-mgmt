
deps = [
    cereal_dep,
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    boost_dep,
    systemd,
]

executable(
    'phosphor-fw-update-vr',
    'main.cpp',
    'vr_code_updater.cpp',
    '../../common/code_updater.cpp',
    '../../common/software.cpp',
    'i2c.cpp',
    'vr.cpp',
    'kv.cpp',
    include_directories: ['../../..'],
    dependencies: deps,
    install: true
)
