subdir('create_package')

gtest_main = dependency('gtest_main', main: true, required: true)

subdir('common')
subdir('spi-flash')

if get_option('cpld-software-update').allowed()
    subdir('cpld')
endif
