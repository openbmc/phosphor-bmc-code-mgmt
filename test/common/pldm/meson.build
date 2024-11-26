
testcases = [
  'test_pldm_package_parser',
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
        gtest,
      ],
      link_with: [
        libpldmutil,
        libpldmcreatepkg,
      ]
    )
  )
endforeach

