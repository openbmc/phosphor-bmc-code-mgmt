AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	phosphor-version-software-manager

phosphor_version_software_manager_SOURCES = \
	version_software_manager.cpp \
	xyz.openbmc_project.Software.Version.cpp \
	version_software_manager_main.cpp \
	activation_software_manager.cpp \
	xyz.openbmc_project.Software.Activation.cpp

phosphor_version_software_manager_LDFLAGS = $(SYSTEMD_LIBS)
