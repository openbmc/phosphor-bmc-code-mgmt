i2c_lib = static_library(
    'i2c_dev',
    'i2c.cpp',
    link_args : '-li2c',
)

i2c_lib_inc = include_directories('.')
i2c_lib_dep = declare_dependency(
    link_with: libi2c_dev,
    include_directories : libi2c_inc,
    link_args : '-li2c')

