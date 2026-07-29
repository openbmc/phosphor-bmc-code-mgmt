# USB RCM recovery driver: the NVIDIA Vera-CPU USB Recovery-Mode protocol done
# in-code over libusb (device match, ECID read, bulk image streaming, boot
# progress-code polling) plus GPIO force-recovery straps via libgpiod. No
# external tool. Dependency-light so both the CLI tool and the async daemon can
# link it.
usb_rcm_lib = static_library(
    'usb-rcm',
    'usb_rcm_driver.cpp',
    dependencies: [libgpiod_dep, libusb_dep],
)

usb_rcm_dep = declare_dependency(
    link_with: usb_rcm_lib,
    include_directories: include_directories('.'),
    dependencies: [libgpiod_dep, libusb_dep],
)

if get_option('usb-rcm-recovery-tool').allowed()
    if cpp.has_header('CLI/CLI.hpp')
        cli11_dep = declare_dependency()
    else
        cli11_dep = dependency('CLI11')
    endif

    executable(
        'nvidia-usb-rcm-recovery-tool',
        'usb_rcm_tool.cpp',
        dependencies: [usb_rcm_dep, cli11_dep],
        install: true,
        install_dir: get_option('bindir'),
    )
endif

if get_option('usb-rcm-recovery-software-update').allowed()
    executable(
        'phosphor-usb-rcm-recovery-software-update',
        'usb_rcm_software_manager.cpp',
        'usb_rcm_device.cpp',
        include_directories: [common_include],
        dependencies: [
            sdbusplus_dep,
            phosphor_logging_dep,
            pdi_dep,
            boost_dep,
            libpldm_dep,
            usb_rcm_dep,
            dependency('threads'),
        ],
        link_with: [software_common_lib, libpldmutil],
        install: true,
        install_dir: get_option('libexecdir') / 'phosphor-code-mgmt',
    )

    install_data(
        'xyz.openbmc_project.Software.USBRCMRecovery.service',
        install_dir: systemd_system_unit_dir,
    )
endif
