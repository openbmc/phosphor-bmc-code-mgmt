i2c_vr_src = files(
    'i2c_vr_device.cpp',
    'i2c_vr_device_code_updater.cpp',
)

i2c_vr_include = include_directories('.')

subdir('regulators')

executable(
    'phosphor-fw-update-vr',
    'main.cpp',
    i2c_vr_src,
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
      libfwupdate,
      libpldmutil,
    ],
    install : true
  )
