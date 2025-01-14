project('phosphor-bmc-code-mgmt', 'cpp',
    default_options: [
        'buildtype=debugoptimized',
        'cpp_std=c++23',
        'warning_level=3',
        'werror=true'
    ],
    meson_version: '>=1.1.1',
    license: 'Apache-2.0',
    version: '1.0')

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
    required: false)
if not has_cereal
    cereal_opts = import('cmake').subproject_options()
    cereal_opts.add_cmake_defines({'BUILD_TESTS': 'OFF', 'SKIP_PERFORMANCE_COMPARISON': 'ON'})
    cereal_proj = import('cmake').subproject(
        'cereal',
        options: cereal_opts,
        required: false)
    assert(cereal_proj.found(), 'cereal is required')
    cereal_dep = cereal_proj.dependency('cereal')
endif

deps = [
    cereal_dep,
    pdi_dep,
    phosphor_logging_dep,
    sdbusplus_dep,
]

ssl = dependency('openssl')

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_variable('systemdsystemunitdir')

build_tests = get_option('tests')

subdir('bmc')

common_include = include_directories('.')

common_build = build_tests.allowed()

if common_build
  libpldm_dep = dependency('libpldm')

  libgpiod = dependency(
    'libgpiodcxx',
    default_options: ['bindings=cxx'],
    version: '>=1.1.2'
  )

  subdir('common')
endif

if build_tests.allowed()
  subdir('test')
endif

