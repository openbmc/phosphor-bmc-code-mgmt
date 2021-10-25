if cpp.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency(
        'CLI11',
        fallback: [ 'CLI11', 'CLI11_dep' ],
    )
endif

source = [
    'usb_code_update_main.cpp',
    'usb_manager.cpp',
    ]

phosphor_logging_dep = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

executable(
    'phosphor-usb-code-update',
    source,
    include_directories: ['..'],
    dependencies: [
        CLI11_dep,
        phosphor_logging_dep,
    ],
    install: true,
    install_dir: get_option('bindir')
)
