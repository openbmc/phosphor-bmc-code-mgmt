libpldm_dep = dependency('libpldm')

common_include = include_directories('.', '../')

subdir('common')
subdir('tool')
subdir('component/pldm-update-pkg-tool')

libgpiod = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: '>=1.1.2'
)

subdir('component/bios-spi')


if get_option('i2cvr').enabled()
  subdir('component/i2c-vr')
  subdir('interface/i2c/')
endif

if not build_tests.disabled()
  subdir('test')
endif

subdir('component/regulators')

