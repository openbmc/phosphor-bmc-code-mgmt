AM_CPPFLAGS = -I$(top_srcdir)

# gtest unit tests which run during a 'make check'
check_PROGRAMS = utest

# Run all 'check' test programs
TESTS = $(check_PROGRAMS)

# Build/add utest to test suite
utest_CPPFLAGS = -Igtest $(GTEST_CPPFLAGS) $(AM_CPPFLAGS)
utest_CXXFLAGS = $(PTHREAD_CFLAGS) $(PHOSPHOR_LOGGING_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	-DUBIFS_LAYOUT
utest_LDFLAGS = -lgtest_main -lgtest $(PTHREAD_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) $(OESDK_TESTCASE_FLAGS) \
	$(PHOSPHOR_LOGGING_LIBS) -lstdc++fs -lssl -lcrypto

utest_SOURCES = utest.cpp

utest_LDADD = \
        $(top_builddir)/image_verify.cpp \
        $(top_builddir)/version.cpp
