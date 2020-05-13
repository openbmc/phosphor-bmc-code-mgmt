image_updater_sources += files(
    'flash.cpp',
    'item_updater_helper.cpp'
)

mmc_unit_files = [
    'obmc-flash-mmc@.service',
]

foreach u : mmc_unit_files
    configure_file(
        copy: true,
        input: u,
        install: true,
        install_dir: systemd_system_unit_dir,
        output: u,
    )
endforeach
