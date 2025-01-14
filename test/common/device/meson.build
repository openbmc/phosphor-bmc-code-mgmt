
testcases = [
  'test_device_specific_update_function',
  'test_device_start_update_async_immediate_success',
  'test_device_start_update_async_onreset_success',
  'test_device_start_update_async_invalid_fd',
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
        gtest,
      ],
      link_with: [libpldmutil, libpldmcreatepkg, software_common_lib, libexampledevice]
    )
  )
endforeach

