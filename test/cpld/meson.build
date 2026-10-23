testcases = ['lattice_chip_str']

foreach t : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            include_directories: [common_include],
            dependencies: [
                libi2c_dep,
                phosphor_logging_dep,
                sdbusplus_dep,
                gtest,
            ],
        ),
    )
endforeach

test(
    'max10_test',
    executable(
        'max10_test',
        'max10_test.cpp',
        '../../cpld/altera/max10_standard_cpld.cpp',
        include_directories: [common_include],
        dependencies: [
            phosphor_logging_dep,
            sdbusplus_dep,
            gtest,
        ],
    ),
)
