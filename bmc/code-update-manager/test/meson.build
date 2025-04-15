gtest_dep = dependency('gtest', main: true)
gmock_dep = dependency('gmock')

# Common include directories
test_includes = [
    include_directories('..'),
    include_directories('../..'),
    include_directories('../../../common/include')
]

# Common dependencies
test_deps = [
    gtest_dep,
    gmock_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
    dependency('threads'),
]

# Tests for code update manager functionality
test_sources = [
  'test_code_update_manager.cpp',
  '../update_manager.cpp',
  'pldm_mock.cpp',
]

test_update_manager = executable(
  'test-update-manager',
  test_sources,
  include_directories: test_includes,
  dependencies: test_deps,
  link_with: [software_common_lib],
  c_args: ['-DTEST'],
  cpp_args: ['-DTEST'],
)

test(
  'test-update-manager',
  test_update_manager,
  env: {
    'BOOST_TEST_COLOR_OUTPUT': '1',
    'BOOST_TEST_LOG_LEVEL': 'all',
  },
)
