testcases = {
    'lattice_chip_str': [],
    'lattice_busy_flag': ['../../cpld/lattice/lattice_base_cpld.cpp'],
}

foreach t, extra_sources : testcases
    test(
        t,
        executable(
            t,
            [f'@t@.cpp'] + extra_sources,
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
