project(
    'phosphor-bmc-code-mgmt',
    'cpp',
    default_options: [
        'buildtype=debugoptimized',
        'cpp_std=c++23',
        'warning_level=3',
        'werror=true',
    ],
    meson_version: '>=1.1.1',
    license: 'Apache-2.0',
    version: '1.0',
)

add_project_arguments(
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_ASIO_DISABLE_THREADS',
    '-DBOOST_ASIO_NO_DEPRECATED',
    language: 'cpp',
)

cpp = meson.get_compiler('cpp')

boost_dep = dependency('boost')

sdbusplus_dep = dependency('sdbusplus')
sdbusplusplus_prog = find_program('sdbus++', native: true)
sdbuspp_gen_meson_prog = find_program('sdbus++-gen-meson', native: true)

pdi_dep = dependency('phosphor-dbus-interfaces')
phosphor_logging_dep = dependency('phosphor-logging')

cereal_dep = dependency('cereal', required: false)
has_cereal = cpp.has_header_symbol(
    'cereal/cereal.hpp',
    'cereal::specialize',
    dependencies: cereal_dep,
    required: false,
)
if not has_cereal
    cereal_opts = import('cmake').subproject_options()
    cereal_opts.add_cmake_defines(
        {'BUILD_TESTS': 'OFF', 'SKIP_PERFORMANCE_COMPARISON': 'ON'},
    )
    cereal_proj = import('cmake').subproject(
        'cereal',
        options: cereal_opts,
        required: false,
    )
    assert(cereal_proj.found(), 'cereal is required')
    cereal_dep = cereal_proj.dependency('cereal')
endif

deps = [cereal_dep, pdi_dep, phosphor_logging_dep, sdbusplus_dep]

ssl_dep = dependency('openssl')

systemd_dep = dependency('systemd')
systemd_system_unit_dir = systemd_dep.get_variable(
    'systemd_system_unit_dir',
    pkgconfig_define: ['prefix', get_option('prefix')],
)

build_tests = get_option('tests')

common_include = include_directories('.')

subdir('bmc')


all_options = {
    'bios-software-update': {'dirs' : ['bios'], 'deps': ['libgpiod']},
    'cpld-software-update': {'dirs' : ['common/i2c', 'cpld']},
    'eepromdevice-software-update': {
        'dirs': ['eeprom-device'],
        'deps': ['libgpiod'],
    },
    'i2cvr-software-update': {'dirs' : ['common/i2c', 'i2c-vr']},
}

optioned_subdirs = []
optioned_deps = []
common_build = false

foreach option_name, option_settings : all_options
    if get_option(option_name).allowed()
        common_build = true

        foreach dir : option_settings.get('dirs', [])
            if not optioned_subdirs.contains(dir)
                optioned_subdirs += dir
            endif
        endforeach

        foreach dep : option_settings.get('deps', [])
            if not optioned_deps.contains(dep)
                optioned_deps += dep
            endif
        endforeach
    endif
endforeach

if common_build or build_tests.allowed()
    libpldm_dep = dependency('libpldm')
    subdir('common')
endif

if optioned_deps.contains('libgpiod')
    libgpiod_dep = dependency(
        'libgpiodcxx',
        default_options: ['bindings=cxx'],
        version: '>=1.1.2',
    )
endif

foreach dir : optioned_subdirs
    subdir(dir)
endforeach

if build_tests.allowed()
    subdir('test')
endif
