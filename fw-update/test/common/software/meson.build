
testcases = [
  'test_software',
]

foreach t : testcases
  test(
    t,
    executable(
      t,
      f'@t@.cpp',
      '../device/nopdevice.cpp',
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
        libfwupdate]
    )
  )
endforeach

