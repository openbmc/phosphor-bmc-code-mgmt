
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
      common_src,
      include_directories: [
        common_include,
      ],
      dependencies: [
        common_deps,
      ],
      link_with: [libpldmutil, libpldmcreatepkg]
    )
  )
endforeach

