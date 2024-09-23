subdir('device')
subdir('software')

testcases = [
  'test_pldm_package_parser',
  'test_pldm_package_verifier',
  'test_pldm_package_get_iana',
  'test_pldm_package_get_compatible',
]


foreach t : testcases
  test(
    t,
    executable(
      t,
      f'@t@.cpp',
      include_directories: [
        common_include,
      ],
      dependencies: [
        libpldm_dep,
        sdbusplus_dep,
        phosphor_logging_dep,
      ],
      link_with: [
        libpldmutil,
        libpldmcreatepkg,
      ]
    )
  )
endforeach

