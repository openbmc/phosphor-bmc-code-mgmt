
common_src = [
  '../../common/fw_manager.cpp',
  '../../common/software.cpp',
  '../../common/pldm_fw_update_verifier.cpp',
]

gpiodcxx = dependency(
  'libgpiodcxx',
  default_options: ['bindings=cxx'],
  version: '>=2.1',
)

common_deps = [
  pdi_dep,
  phosphor_logging_dep,
  sdbusplus_dep,
  boost_dep,
  systemd,
  libpldm_dep,
]

common_include = '../../..'

subdir('component/bios-spi')
subdir('component/voltage-regulator')

