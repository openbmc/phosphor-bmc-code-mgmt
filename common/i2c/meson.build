libi2c_dev = static_library('i2c_dev', 'i2c.cpp', link_args: '-li2c')
libi2c_inc = include_directories('.')
libi2c_dep = declare_dependency(
    common_include,
    link_with: libi2c_dev,
    include_directories: libi2c_inc,
    link_args: '-li2c',
)
