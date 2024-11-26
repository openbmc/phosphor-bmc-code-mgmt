
testcases = [
  'test_pldm_package_get_iana',
  'test_pldm_package_get_compatible',
  'test_pldm_package_get_component_image',
  'test_pldm_package_get_firmware_device_id',
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
        phosphor_logging_dep,
      ],
      link_with: [
        libpldmutil,
        libpldmcreatepkg,
      ]
    )
  )
endforeach

