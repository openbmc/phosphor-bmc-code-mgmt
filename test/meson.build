oe_sdk = get_option('oe-sdk')
if oe_sdk.enabled()
  # Setup OE SYSROOT
  OECORE_TARGET_SYSROOT = run_command('sh', '-c', 'echo $OECORE_TARGET_SYSROOT').stdout().strip()
  if OECORE_TARGET_SYSROOT == ''
      error('Unable to get $OECORE_TARGET_SYSROOT, check your environment.')
  endif
  message('OE_SYSROOT: ' + OECORE_TARGET_SYSROOT)
  rpath = ':'.join([OECORE_TARGET_SYSROOT + '/lib', OECORE_TARGET_SYSROOT + '/usr/lib'])
  ld_so = run_command('sh', '-c', 'find ' + OECORE_TARGET_SYSROOT + '/lib/ld-*.so | sort -r -n | head -n1').stdout().strip()
  dynamic_linker = ['-Wl,-dynamic-linker,' + ld_so]
else
  dynamic_linker = []
endif

gtest = dependency('gtest', main: true, disabler: true, required: build_tests)
include_dirs = declare_dependency(include_directories: '..')
include_srcs = declare_dependency(sources: [
    '../image_verify.cpp',
    '../version.cpp']
  )

test('utest',
  executable(
    'utest',
    'utest.cpp',
    link_args: dynamic_linker,
    build_rpath: get_option('oe-sdk').enabled() ? rpath : '',
    dependencies: [deps, gtest, include_dirs, include_srcs, ssl]
  )
)
