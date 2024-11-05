
common_include = include_directories('.', '../')

subdir('common')

gpiodcxx = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: ['>=1.6.3', '<2.0.0'],
)

subdir('component/bios-spi')
subdir('component/pldm-update-pkg-tool')
subdir('component/vr-i2c')
subdir('component/interfaces/i2c')
subdir('component/regulators')

subdir('test')

