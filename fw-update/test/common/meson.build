
verifier_test = executable(
    'test_pldm_package_verifier',
    'test_pldm_package_verifier.cpp',
    'create_pldm_fw_package.cpp',
    common_src,
    include_directories: [
      common_include,
    ],
    dependencies: [
      common_deps,
    ]
)

parser_test = executable(
    'test_pldm_package_parser',
    'test_pldm_package_parser.cpp',
    'create_pldm_fw_package.cpp',
    common_src,
    include_directories: [
      common_include,
    ],
    dependencies: [
      common_deps,
    ]
)

test(
        'test_pldm_package_verifier',
        verifier_test,
        workdir: meson.current_source_dir(),
)

test(
        'test_pldm_package_parser',
        parser_test,
        workdir: meson.current_source_dir(),
)
