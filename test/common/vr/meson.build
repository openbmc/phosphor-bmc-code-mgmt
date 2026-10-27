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
    'tps25990_image',
    executable(
        'tps25990_image',
        'tps25990_image_unittest.cpp',
        '../../../i2c-vr/tps25990/tps25990.cpp',
        include_directories: [common_include],
        dependencies: [
            libi2c_dep,
            sdbusplus_dep,
            phosphor_logging_dep,
            gtest,
        ],
    ),
)
