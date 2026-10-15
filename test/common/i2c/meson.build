test(
    'i2c_fd_test',
    executable(
        'i2c_fd_test',
        'i2c_fd_test.cpp',
        '../../../common/i2c/i2c.cpp',
        include_directories: [libi2c_inc],
        dependencies: [sdbusplus_dep, gtest_main, gtest],
        link_args: '-li2c',
    ),
)
