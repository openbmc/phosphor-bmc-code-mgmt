testcases = ['package_util']

foreach t : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            include_directories: [common_include, '../../../common/include'],
            dependencies: [
                libpldm_dep,
                libpldmcpp_dep,
                sdbusplus_dep,
                phosphor_logging_dep,
                gtest,
            ],
            link_with: [libpldmutil, libpldmcreatepkg],
        ),
    )
endforeach
