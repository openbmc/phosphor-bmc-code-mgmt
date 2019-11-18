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
	item_updater_helper.hpp \
	utils.hpp

bin_PROGRAMS = \
	phosphor-version-software-manager \
	phosphor-download-manager \
	phosphor-image-updater

dist_bin_SCRIPTS = \
	obmc-flash-bmc

phosphor_version_software_manager_SOURCES = \
	image_manager_main.cpp \
	watch.cpp \
	version.cpp \
	image_manager.cpp

BUILT_SOURCES = \
	xyz/openbmc_project/Software/Image/error.cpp \
	xyz/openbmc_project/Software/Image/error.hpp

CLEANFILES = ${BUILT_SOURCES}

phosphor_download_manager_SOURCES = \
	download_manager.cpp \
	download_manager_main.cpp

phosphor_image_updater_SOURCES = \
	activation.cpp \
	version.cpp \
	serialize.cpp \
	item_updater.cpp \
	item_updater_main.cpp \
	utils.cpp

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	obmc-flash-bmc-setenv@.service \
	usr-local.mount \
	xyz.openbmc_project.Software.BMC.Updater.service \
	xyz.openbmc_project.Software.Download.service \
	xyz.openbmc_project.Software.Sync.service \
	xyz.openbmc_project.Software.Version.service \
	reboot-guard-disable.service \
	reboot-guard-enable.service \
	force-reboot.service

tmpfilesdir=${exec_prefix}/lib/tmpfiles.d/
dist_tmpfiles_DATA = \
	software.conf
endif

if UBIFS_LAYOUT
include ubi/Makefile.am.include
else
include static/Makefile.am.include
endif

if WANT_SIGNATURE_VERIFY_BUILD
noinst_HEADERS += \
	image_verify.hpp \
	openssl_alloc.hpp
phosphor_image_updater_SOURCES += \
	image_verify.cpp \
	openssl_alloc.cpp
endif

if WANT_SYNC
noinst_HEADERS += \
	sync_manager.hpp \
	sync_watch.hpp
bin_PROGRAMS += phosphor-sync-software-manager
phosphor_sync_software_manager_SOURCES = \
	sync_manager.cpp \
	sync_watch.cpp \
	sync_manager_main.cpp
phosphor_sync_software_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_sync_software_manager_LDFLAGS = $(generic_ldflags)
dist_sysconf_DATA = \
	synclist
endif

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

phosphor_version_software_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_version_software_manager_LDFLAGS = $(generic_ldflags)
phosphor_download_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_download_manager_LDFLAGS = $(generic_ldflags)
phosphor_image_updater_CXXFLAGS = $(generic_cxxflags)
phosphor_image_updater_LDFLAGS = $(generic_ldflags)

SUBDIRS = test
