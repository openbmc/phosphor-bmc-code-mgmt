AM_DEFAULT_SOURCE_EXT = .cpp

# Build these headers, don't install them
noinst_HEADERS = \
	download_manager.hpp \
	watch.hpp \
	version.hpp \
	images.hpp \
	image_manager.hpp \
	item_updater.hpp \
	activation.hpp \
	flash.hpp \
	item_updater_helper.hpp

sbin_PROGRAMS = \
	phosphor-version-software-manager \
	phosphor-download-manager \
	phosphor-image-updater

phosphor_version_software_manager_SOURCES = \
	image_manager_main.cpp \
	watch.cpp \
	version.cpp \
	image_manager.cpp

BUILT_SOURCES = \
	xyz/openbmc_project/Software/Image/error.cpp \
	xyz/openbmc_project/Software/Image/error.hpp \
	org/openbmc/Associations/server.cpp \
	org/openbmc/Associations/server.hpp

CLEANFILES = ${BUILT_SOURCES}

phosphor_download_manager_SOURCES = \
	download_manager.cpp \
	download_manager_main.cpp

phosphor_image_updater_SOURCES = \
	activation.cpp \
	version.cpp \
	serialize.cpp \
	item_updater.cpp \
	item_updater_main.cpp

if UBIFS_LAYOUT
include ubi/Makefile.am.include
else
include static/Makefile.am.include
endif

if WANT_SIGNATURE_VERIFY_BUILD
noinst_HEADERS += \
	image_verify.hpp \
	utils.hpp
phosphor_image_updater_SOURCES += \
	image_verify.cpp \
	utils.cpp
endif

if WANT_SYNC
noinst_HEADERS += \
	sync_manager.hpp \
	sync_watch.hpp
sbin_PROGRAMS += phosphor-sync-software-manager
phosphor_sync_software_manager_SOURCES = \
	sync_manager.cpp \
	sync_watch.cpp \
	sync_manager_main.cpp
phosphor_sync_software_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_sync_software_manager_LDFLAGS = $(generic_ldflags)
endif

nodist_phosphor_image_updater_SOURCES = \
	org/openbmc/Associations/server.cpp

generic_cxxflags = \
	$(SYSTEMD_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	-flto
generic_ldflags = \
	$(SYSTEMD_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	-lstdc++fs \
	-lssl \
	-lcrypto

xyz/openbmc_project/Software/Image/error.hpp: ${top_srcdir}/xyz/openbmc_project/Software/Image.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) error exception-header xyz.openbmc_project.Software.Image > $@

xyz/openbmc_project/Software/Image/error.cpp: ${top_srcdir}/xyz/openbmc_project/Software/Image.errors.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) error exception-cpp xyz.openbmc_project.Software.Image > $@

org/openbmc/Associations/server.cpp: org/openbmc/Associations.interface.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) interface server-cpp org.openbmc.Associations > $@

org/openbmc/Associations/server.hpp: org/openbmc/Associations.interface.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) interface server-header org.openbmc.Associations > $@

phosphor_version_software_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_version_software_manager_LDFLAGS = $(generic_ldflags)
phosphor_download_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_download_manager_LDFLAGS = $(generic_ldflags)
phosphor_image_updater_CXXFLAGS = $(generic_cxxflags)
phosphor_image_updater_LDFLAGS = $(generic_ldflags)

SUBDIRS = test
