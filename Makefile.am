AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	op-level-software-manager

op_level_software_manager_SOURCES = \
	level_software_manager.cpp \
	xyz.openbmc_project.Software.Level.cpp \
	level_software_manager_main.cpp

op_level_software_manager_LDFLAGS = $(SYSTEMD_LIBS)
