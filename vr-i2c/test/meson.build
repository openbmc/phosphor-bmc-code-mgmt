executable(
      'i2c-vrtest',
      'main.cpp',
      i2c_vr_src,
      include_directories: [
        common_include,
        i2c_vr_include,
        libi2c_inc,
      ],
      dependencies: [
          phosphor_logging_dep,
          libi2c_dep,
      ],
)
