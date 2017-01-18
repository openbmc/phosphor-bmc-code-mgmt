AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	phosphor-level-software-manager

phosphor_level_software_manager_SOURCES = \
	level_software_manager.cpp \
	xyz.openbmc_project.Software.Level.cpp \
	level_software_manager_main.cpp

phosphor_level_software_manager_LDFLAGS = $(SYSTEMD_LIBS)
