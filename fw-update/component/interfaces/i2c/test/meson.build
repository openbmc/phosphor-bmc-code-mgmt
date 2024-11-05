libi2c_dev_mock_inc = include_directories('.')

libi2c_dev_mock = static_library(
    'i2c_dev_mock',
    'mocked_i2c_interface.cpp',
    dependencies: [
        gmock
    ],
    link_args: dynamic_linker,
    build_rpath: get_option('oe-sdk').allowed() ? rpath : '',
    include_directories: [
        libi2c_inc,
        libi2c_dev_mock_inc
    ]
)
