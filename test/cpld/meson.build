lattice_objs = exe.extract_objects(
    'lattice/lattice_base_cpld.cpp',
)

testcases = {
    'lattice_chip_str': [],
    'lattice_jed_parser': [lattice_objs],
}

foreach t, extra_objs : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            objects: extra_objs,
            include_directories: [common_include, libi2c_inc],
            dependencies: [
                libi2c_dep,
                phosphor_logging_dep,
                sdbusplus_dep,
                gtest,
            ],
            link_with: [software_common_lib, libi2c_dev],
        ),
    )
endforeach
