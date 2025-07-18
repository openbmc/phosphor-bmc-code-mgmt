
testcases = ['software_get_random_softwareid', 'software_config']

foreach t : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            include_directories: [common_include, '../device/'],
            dependencies: [
                libpldm_dep,
                sdbusplus_dep,
                phosphor_logging_dep,
                gtest,
            ],
            link_with: [
                libpldmutil,
                libpldmcreatepkg,
                software_common_lib,
                libexampledevice,
            ],
        ),
    )
endforeach

