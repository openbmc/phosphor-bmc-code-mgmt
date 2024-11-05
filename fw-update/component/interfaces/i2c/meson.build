libi2c_dev = static_library(
    'i2c_dev',
    'i2c.cpp',
    link_args : '-li2c',
)

libi2c_inc = include_directories('.')
libi2c_dep = declare_dependency(
    link_with: libi2c_dev,
    include_directories : libi2c_inc,
    link_args : '-li2c')

if get_option('tests').allowed()
    subdir('test')
endif
