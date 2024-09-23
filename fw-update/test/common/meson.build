
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

test(
        'test_pldm_package_verifier',
        verifier_test,
        workdir: meson.current_source_dir(),
)
