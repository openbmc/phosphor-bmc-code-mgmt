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
    'tps544x27_csv_parser',
    executable(
        'tps544x27_csv_parser',
        'tps544x27_csv_parser_unittest.cpp',
        '../../../i2c-vr/tps544x27/csv_parser.cpp',
        '../../../common/src/utils.cpp',
        include_directories: [common_include],
        dependencies: [
            phosphor_logging_dep,
            gtest_main,
        ],
    ),
)