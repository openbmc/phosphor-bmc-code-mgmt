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
