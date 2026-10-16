subdir('create_package')

gtest_main = dependency('gtest_main', main: true, required: true)

subdir('common')

if get_option('i2cvr-software-update').allowed()
    subdir('i2c-vr')
endif

if get_option('cpld-software-update').allowed()
    subdir('cpld')
endif
