test(
    'utils_async_system',
    executable(
        'utils_async_system',
        'utils_async_system.cpp',
        include_directories: [common_include],
        dependencies: [
            sdbusplus_dep,
            phosphor_logging_dep,
            gtest,
        ],
        link_with: [
            software_common_lib,
        ],
    ),
)
