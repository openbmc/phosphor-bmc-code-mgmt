# UTEST

Instructions on how to execute UTEST.

- When using an SDK - make sure it has been built for an x86 machine.

  Run the following commands:

  1. meson -Dtests=enabled build
  2. ninja -C build test

- WHEN RUNNING UTEST remember to take advantage of the gtest capabilities.
  "./build/test/utest --help"
  - --gtest_repeat=[COUNT]
  - --gtest_shuffle
  - --gtest_random_seed=[NUMBER]
