source = [
    'side_switch.cpp',
    '../utils.cpp',
    ]

executable(
    'phosphor-bmc-side-switch',
    source,
    include_directories: ['..'],
    dependencies: [
        phosphor_logging_dep,
    ],
    install: true,
    install_dir: get_option('bindir')
)
