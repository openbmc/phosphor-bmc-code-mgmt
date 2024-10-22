vr_dev_src = files(
    'vr.cpp',
    'xdpe152xx.cpp',
)

vr_dev_include = include_directories('.')

executable(
      'i2c-vrtest',
      'main.cpp',
      vr_dev_src,
      include_directories: [
        common_include,
        vr_dev_include,
        libi2c_inc,
      ],
      dependencies: [
          phosphor_logging_dep,
          libi2c_dep,
      ],
)
