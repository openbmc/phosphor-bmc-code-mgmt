
common_include = include_directories('.', '../')

subdir('common')

libgpiod = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: '>=1.1.2'
)

subdir('tool')
subdir('component/pldm-update-pkg-tool')

if not build_tests.disabled()
  subdir('test')
endif

