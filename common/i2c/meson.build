libi2c_inc = include_directories('../include/i2c/')
libi2c_dev = static_library(
    'i2c_dev',
    'i2c.cpp',
    dependencies: [sdbusplus_dep],
    include_directories: libi2c_inc,
    link_args: '-li2c',
)
libi2c_dep = declare_dependency(
    link_with: libi2c_dev,
    dependencies: [sdbusplus_dep],
    include_directories: libi2c_inc,
    link_args: '-li2c',
)
