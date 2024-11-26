libpldm_dep = dependency('libpldm')

common_include = include_directories('.', '../')

subdir('common')
subdir('tool')

if not build_tests.disabled()
  subdir('test')
endif

