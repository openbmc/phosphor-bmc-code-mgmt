
testcases = ['events']

foreach t : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            include_directories: [common_include],
            dependencies: [
                pdi_dep,
                sdbusplus_dep,
                phosphor_logging_dep,
                gtest,
            ],
            link_with: [
                software_common_lib,
            ],
        ),
        is_parallel: false,
    )
endforeach
