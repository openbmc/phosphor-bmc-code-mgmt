test(
    'xdpe1x2xx_image',
    executable(
        'xdpe1x2xx_image',
        'xdpe1x2xx_image_unittest.cpp',
        '../../../i2c-vr/xdpe1x2xx/xdpe1x2xx.cpp',
        include_directories: [common_include],
        dependencies: [
            libi2c_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            gtest,
        ],
    ),
)

test(
    'tda38640a_image',
    executable(
        'tda38640a_image',
        'tda38640a_image_unittest.cpp',
        '../../../i2c-vr/tda38640a/tda38640a.cpp',
        include_directories: [common_include],
        dependencies: [
            libi2c_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            gtest,
        ],
    ),
)
