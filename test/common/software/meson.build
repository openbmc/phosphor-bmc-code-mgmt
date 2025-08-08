
testcases = [
    'software_get_random_softwareid',
    'software_config',
    'software_association',
    'software_version',
]

foreach t : testcases
    test(
        t,
        executable(
            t,
            f'@t@.cpp',
            include_directories: [common_include],
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

