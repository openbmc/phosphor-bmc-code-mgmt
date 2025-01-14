
testcases = [
  'test_software',
  'test_software_association',
  'test_software_version',
  'test_software_get_random_softwareid',
]

foreach t : testcases
  test(
    t,
    executable(
      t,
      f'@t@.cpp',
      include_directories: [
        common_include,
        '../device/',
      ],
      dependencies: [
        libpldm_dep,
        sdbusplus_dep,
        phosphor_logging_dep,
        gtest,
      ],
      link_with: [
        libpldmutil,
        software_common_lib,
        libnopdevice,
      ]
    )
  )
endforeach

