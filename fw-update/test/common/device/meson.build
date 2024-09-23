
testcases = [
  'test_device_specific_update_function',
  'test_device_start_update_success',
  'test_device_start_update_invalid_fd',
]

foreach t : testcases
  test(
    t,
    executable(
      t,
      f'@t@.cpp',
      'nopdevice.cpp',
      include_directories: [
        common_include,
      ],
      dependencies: [
        libpldm_dep,
        sdbusplus_dep,
        phosphor_logging_dep,
        gtest,
      ],
      link_with: [libpldmutil, libpldmcreatepkg, libfwupdate]
    )
  )
endforeach

