vr_dev_src = files(
    'vr.cpp',
    'xdpe152xx.cpp'
)

vr_dev_include = include_directories('.')

executable(
      'i2c-vrtest',
      'main.cpp',
      vr_dev_src,
      dependencies: [
          libi2c_dev,
          phosphor_logging_dep,
      ]
)
