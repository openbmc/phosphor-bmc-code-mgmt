i2c_vr_src = files(
    'i2c_vr_device.cpp',
    'i2c_vr_device_code_updater.cpp',
    'vr.cpp',
)

regulators_src = files(
  'xdpe1x2xx/xdpe1x2xx.cpp',
)

i2c_vr_include = include_directories('.')

executable(
    'phosphor-fw-update-vr',
    'main.cpp',
    i2c_vr_src,
    regulators_src,
    include_directories: [
      common_include,
      i2c_vr_include,
    ],
    dependencies: [
      sdbusplus_dep,
      phosphor_logging_dep,
      pdi_dep,
      boost_dep,
      libpldm_dep,
    ],
    link_with: [
      software_common_lib,
      libpldmutil,
    ],
    install : true
  )
