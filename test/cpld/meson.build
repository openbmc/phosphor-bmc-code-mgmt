lattice_objs = cpld_exe.extract_objects(
    '../../cpld/lattice/lattice_base_cpld.cpp',
)

testcases = {
    'lattice_chip_str': [],
    'lattice_jed_parser': [lattice_objs],
}

foreach t, extra_objects : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            objects: extra_objects,
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
