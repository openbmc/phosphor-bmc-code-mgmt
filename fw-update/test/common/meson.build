subdir('nopdevice')
subdir('device')
subdir('software')

testcases = [
  'test_pldm_package_parser',
  'test_pldm_package_verifier',
  'test_pldm_package_get_iana',
  'test_pldm_package_get_compatible',
  'test_device_config',
  'test_software_update',
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
        gtest_main,
      ],
      link_with: [
        libpldmutil,
        libpldmcreatepkg,
        libfwupdate,
        libnopdevice,
      ]
    )
  )
endforeach

