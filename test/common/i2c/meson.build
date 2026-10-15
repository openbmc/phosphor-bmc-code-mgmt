i2c_fd_cpp_args = []
i2c_fd_link_args = [
    '-Wl,--wrap=open',
    '-Wl,--wrap=ioctl',
    '-Wl,--wrap=close',
]

foreach symbol : ['open64', '__ioctl_time64']
    if cpp.has_function(symbol, prefix: 'extern "C" int ' + symbol + '(...);')
        i2c_fd_cpp_args += '-DHAVE_WRAP_' + symbol.to_upper()
        i2c_fd_link_args += '-Wl,--wrap=' + symbol
    endif
endforeach

test(
    'i2c_fd_test',
    executable(
        'i2c_fd_test',
        'i2c_fd_test.cpp',
        include_directories: [common_include],
        dependencies: [libi2c_dep, gtest],
        cpp_args: i2c_fd_cpp_args,
        link_args: i2c_fd_link_args,
    ),
)
