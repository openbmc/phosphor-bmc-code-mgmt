testcases = ['lattice_busy_flag']

foreach t : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            '../../cpld/lattice/lattice_base_cpld.cpp',
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
