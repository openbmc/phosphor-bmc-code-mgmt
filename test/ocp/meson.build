testcases = ['test_ocp_recovery']

foreach t : testcases
    test(
        t,
        executable(t, f'@t@.cpp', dependencies: [gtest_main, libocp_dep]),
    )
endforeach
