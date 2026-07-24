libserial_inc = include_directories('../include/serial/')
libserial_dev = static_library(
    'serial_dev',
    'serial_port.cpp',
    'serial_terminal.cpp',
    include_directories: [libserial_inc]
)
libserial_dep = declare_dependency(
    include_directories: [libserial_inc]
)
