image_updater_sources += files(
    'flash.cpp',
    'item_updater_helper.cpp'
)

ubi_unit_files = [
    'obmc-flash-bmc-cleanup.service',
    'obmc-flash-bmc-mirroruboot.service',
    'obmc-flash-bmc-ubiremount.service',
    'obmc-flash-bmc-ubiro@.service',
    'obmc-flash-bmc-ubiro-remove@.service',
    'obmc-flash-bmc-ubirw.service',
    'obmc-flash-bmc-ubirw-remove.service',
    'obmc-flash-bmc-updateubootvars@.service'
]

foreach u : ubi_unit_files
    configure_file(
        copy: true,
        input: u,
        install: true,
        install_dir: systemd_system_unit_dir,
        output: u,
    )
endforeach
