conf = configuration_data()

cpld_version_source_active = get_option('cpld-version-source-active')
if cpld_version_source_active
    conf.set('CPLD_VERSION_SOURCE_ACTIVE', '1')
else
    conf.set('CPLD_VERSION_SOURCE_ACTIVE', '0')
endif

configure_file(output: 'config.h', configuration: conf)

cpld_src = files('cpld.cpp', 'cpld_interface.cpp', 'cpld_software_manager.cpp')

cpld_vendor_src = files(
    'lattice/lattice_base_cpld.cpp',
    'lattice/lattice_cpld_factory.cpp',
    'lattice/lattice_xo3_cpld.cpp',
    'lattice/lattice_xo5_cpld.cpp',
)

exe = executable(
    'phosphor-cpld-software-update',
    cpld_src,
    cpld_vendor_src,
    include_directories: [
        include_directories('.'),
        common_include,
        libi2c_inc,
        gpio_inc,
    ],
    dependencies: [
        pdi_dep,
        phosphor_logging_dep,
        sdbusplus_dep,
        libpldm_dep,
        libi2c_dep,
        libgpiod_dep,
    ],
    link_with: [
        libpldmutil,
        software_common_lib,
        libi2c_dev,
        libgpio_controller,
    ],
    link_args: '-li2c',
    install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
    install: true,
)

install_data(
    'xyz.openbmc_project.Software.CPLD.service',
    install_dir: systemd_system_unit_dir,
)
