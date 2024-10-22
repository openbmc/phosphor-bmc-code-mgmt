i2c_vr_src = files(
    'vr_fw.cpp',
    'i2c_vr_device.cpp',
    'i2c_vr_device_code_updater.cpp',
    'vr_fw_udate.cpp',
)

i2c_vr_include = include_directories('.')

executable(
    'phosphor-fw-update-vr',
    'main.cpp',
    i2c_vr_src,
    include_directories: [
      common_include,
      i2c_vr_include,
    ],
    dependencies: [
      common_deps,
      sdbusplus_dep,
      phosphor_logging_dep,
      pdi_dep,
      boost_dep,
    ],
    link_with: [libfwupdate],
    install : true
  )
