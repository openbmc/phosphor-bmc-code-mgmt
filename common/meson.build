
subdir('pldm')

conf = configuration_data()
conf.set(
    'HOST_STATE_TRANSITION_TIMEOUT',
    get_option('host-state-transition-timeout'),
)

configure_file(output: 'common_config.h', configuration: conf)

gpio_inc = include_directories('include')

libgpio_controller = static_library(
    'gpio_controller',
    'src/gpio_controller.cpp',
    include_directories: ['.', 'include', common_include],
    dependencies: [libgpiod_dep, phosphor_logging_dep],
)

software_common_lib = static_library(
    'software_common_lib',
    'src/software_manager.cpp',
    'src/device.cpp',
    'src/software_config.cpp',
    'src/software.cpp',
    'src/software_update.cpp',
    'src/host_power.cpp',
    'src/utils.cpp',
    include_directories: ['.', 'include/', common_include],
    dependencies: [
        pdi_dep,
        phosphor_logging_dep,
        sdbusplus_dep,
        libgpiod_dep,
        libpldm_dep,
    ],
)

# Export dependency for other components to use
software_common_dep = declare_dependency(
    link_with: software_common_lib,
    include_directories: ['.', 'include/', common_include],
    dependencies: [
        pdi_dep,
        phosphor_logging_dep,
        sdbusplus_dep,
        libpldm_dep,
        pldm_package_dep,
    ],
)
