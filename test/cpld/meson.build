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
    'lattice_cpld_test',
    executable(
        'lattice_cpld_test',
        'lattice_cpld_test.cpp',
        '../../cpld/lattice/lattice_base_cpld.cpp',
        '../../cpld/lattice/lattice_xo3_cpld.cpp',
        include_directories: [common_include],
        dependencies: [
            libi2c_dep,
            phosphor_logging_dep,
            sdbusplus_dep,
            gtest,
        ],
    ),
)
