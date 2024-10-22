
common_include = include_directories('.', '../')

subdir('common')

libgpiod = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: '>=1.1.2'
)

subdir('tool')
subdir('component/bios-spi')
subdir('component/pldm-update-pkg-tool')
subdir('component/vr-i2c')

subdir('test')

