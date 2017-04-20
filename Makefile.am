AM_DEFAULT_SOURCE_EXT = .cpp

# Build these headers, don't install them
noinst_HEADERS = \
	version_software_manager.hpp \
	download_manager.hpp \
	watch.hpp \
	version.hpp \
	image_manager.hpp 

sbin_PROGRAMS = \
	phosphor-version-software-manager \
	phosphor-download-manager

phosphor_version_software_manager_SOURCES = \
	version_software_manager.cpp \
	image_manager_main.cpp \
	watch.cpp \
	version.cpp \
	image_manager.cpp

phosphor_download_manager_SOURCES = \
	download_manager.cpp \
	download_manager_main.cpp

generic_cxxflags = \
	$(SYSTEMD_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS)
generic_ldflags = \
	$(SYSTEMD_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS)

phosphor_version_software_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_version_software_manager_LDFLAGS = $(generic_ldflags)
phosphor_download_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_download_manager_LDFLAGS = $(generic_ldflags)

SUBDIRS = test
