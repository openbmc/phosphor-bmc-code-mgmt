if cpp.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency(
        'CLI11',
        fallback: [ 'CLI11', 'CLI11_dep' ],
    )
endif

source = ['usb_code_update_main.cpp']

executable(
    'phosphor-usb-code-update',
    source,
    dependencies: [
        CLI11_dep,
    ],
    install: true,
    install_dir: get_option('bindir')
)
