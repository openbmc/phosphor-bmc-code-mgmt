test(
    'tps544b28_csv_parser',
    executable(
        'tps544b28_csv_parser',
        'csv_parser.cpp',
        '../../../i2c-vr/tps544b28/csv_parser.cpp',
        '../../../common/src/utils.cpp',
        include_directories: [common_include],
        dependencies: [
            phosphor_logging_dep,
            gtest_main,
        ],
    ),
)
