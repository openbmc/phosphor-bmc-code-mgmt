testcases = ['utils']

foreach t : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            '../../../common/src/utils.cpp',
            include_directories: [common_include],
            dependencies: [
                sdbusplus_dep,
                phosphor_logging_dep,
                gtest,
            ],
        ),
    )
endforeach
