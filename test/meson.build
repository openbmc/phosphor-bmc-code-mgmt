subdir('create_package')

gtest_main = dependency('gtest_main', main: true, required: true)

subdir('common')

if get_option('cpld-software-update').allowed()
    subdir('cpld')
endif

if get_option('cpld-software-update').allowed() or get_option(
    'i2cvr-software-update',
).allowed()
    subdir('common/i2c')
endif
