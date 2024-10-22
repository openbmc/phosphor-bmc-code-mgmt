
common_src = files(
  'common/pldm/utils.cpp',
  'common/pldm/package_parser.cpp',
  'common/fw_manager.cpp',
  'common/device.cpp',
  'common/software.cpp',
  'common/software-version.cpp',
  'common/software-activation.cpp',
  'common/software-association-definitions.cpp',
  'common/pldm_fw_update_verifier.cpp',
)

gpiodcxx = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: ['>=1.6.3', '<2.0.0'],
)

common_deps = [
  pdi_dep,
  phosphor_logging_dep,
  sdbusplus_dep,
  boost_dep,
  systemd,
  libpldm_dep,
]

common_include = include_directories('../')

subdir('component/bios-spi')
subdir('component/vr-i2c')

subdir('test')

