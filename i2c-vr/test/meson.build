executable(
      'i2c-vrtest',
      'main.cpp',
      i2c_vr_src,
      include_directories: [
        common_include,
        i2c_vr_include,
        i2c_lib_inc,
      ],
      dependencies: [
          phosphor_logging_dep,
          i2c_lib_dep,
      ],
)
