libpldm_dep = dependency('libpldm')

common_include = include_directories('.', '../')

subdir('common')
subdir('tool')

libgpiod = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: '>=1.1.2'
)

if not build_tests.disabled()
  subdir('test')
endif

