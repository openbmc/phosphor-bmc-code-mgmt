AC_PREREQ([2.69])
AC_INIT([phosphor-bmc-code-mgmt], [1.0], [https://github.com/openbmc/phosphor-bmc-code-mgmt/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

# Checks for programs
AC_PROG_CXX
AC_PROG_INSTALL #Checks/sets the install variable to be used
AC_PROG_MAKE_SET

# Check for libraries
PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 221])
PKG_CHECK_MODULES([PHOSPHOR_DBUS_INTERFACES], [phosphor-dbus-interfaces],,\
    AC_MSG_ERROR(["Requires phosphor-dbus-interfaces package."]))
PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus],,
    AC_MSG_ERROR(["Requires sdbusplus package."]))
PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging],,\
    AC_MSG_ERROR(["Requires phosphor-logging package."]))
# Check for sdbus++
AC_PATH_PROG([SDBUSPLUSPLUS], [sdbus++])
AS_IF([test "x$SDBUSPLUSPLUS" == "x"],
    AC_MSG_ERROR(["Requires sdbus++"]))

# Checks for library functions
LT_INIT # Required for systemd linking

# Check/set gtest specific functions.
AX_PTHREAD([GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"],[GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"])
AC_SUBST(GTEST_CPPFLAGS)
AC_ARG_ENABLE([oe-sdk],
    AS_HELP_STRING([--enable-oe-sdk], [Link testcases absolutely against OE SDK so they can be ran within it.])
)
AC_ARG_VAR(OECORE_TARGET_SYSROOT,
    [Path to the OE SDK SYSROOT])
AS_IF([test "x$enable_oe_sdk" == "xyes"],
    AS_IF([test "x$OECORE_TARGET_SYSROOT" == "x"],
          AC_MSG_ERROR([OECORE_TARGET_SYSROOT must be set with --enable-oe-sdk])
    )
    AC_MSG_NOTICE([Enabling OE-SDK at $OECORE_TARGET_SYSROOT])
    [
        testcase_flags="-Wl,-rpath,\${OECORE_TARGET_SYSROOT}/lib"
        testcase_flags="${testcase_flags} -Wl,-rpath,\${OECORE_TARGET_SYSROOT}/usr/lib"
        testcase_flags="${testcase_flags} -Wl,-dynamic-linker,`find \${OECORE_TARGET_SYSROOT}/lib/ld-*.so | sort -r -n | head -n1`"
    ]
    AC_SUBST([OESDK_TESTCASE_FLAGS], [$testcase_flags])
)

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext])
AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

AC_DEFINE(ACTIVATION_FWD_ASSOCIATION, "inventory", [The name of the activation's forward association.])
AC_DEFINE(ACTIVATION_REV_ASSOCIATION, "activation", [The name of the activation's reverse association.])
AC_DEFINE(INVENTORY_PATH, "/xyz/openbmc_project/inventory/",
    [The inventory path root.])
AC_DEFINE(BMC_INVENTORY_INTERFACE, "xyz.openbmc_project.Inventory.Item.Bmc",
    [The BMC inventory interface])

AC_DEFINE(ACTIVE_FWD_ASSOCIATION, "active", [The name of the active's forward association.])
AC_DEFINE(ACTIVE_REV_ASSOCIATION, "software_version", [The name of the active's reverse association.])

AC_DEFINE(FUNCTIONAL_FWD_ASSOCIATION, "functional", [The name of the functional forward association.])
AC_DEFINE(FUNCTIONAL_REV_ASSOCIATION, "functional", [The name of the functional reverse association.])

AC_ARG_VAR(VERSION_BUSNAME, [The Dbus busname to own])
AS_IF([test "x$VERSION_BUSNAME" == "x"], [VERSION_BUSNAME="xyz.openbmc_project.Software.Version"])
AC_DEFINE_UNQUOTED([VERSION_BUSNAME], ["$VERSION_BUSNAME"], [The DBus busname to own])

AC_ARG_VAR(SOFTWARE_OBJPATH, [The version software manager Dbus root])
AS_IF([test "x$SOFTWARE_OBJPATH" == "x"], [SOFTWARE_OBJPATH="/xyz/openbmc_project/software"])
AC_DEFINE_UNQUOTED([SOFTWARE_OBJPATH], ["$SOFTWARE_OBJPATH"], [The version software manager Dbus root])

AC_ARG_VAR(DOWNLOAD_BUSNAME, [The Dbus busname to own])
AS_IF([test "x$DOWNLOAD_BUSNAME" == "x"],
    [DOWNLOAD_BUSNAME="xyz.openbmc_project.Software.Download"])
AC_DEFINE_UNQUOTED([DOWNLOAD_BUSNAME], ["$DOWNLOAD_BUSNAME"], [The DBus busname to own])

AC_ARG_VAR(SYNC_BUSNAME, [The D-Bus busname to own])
AS_IF([test "x$SYNC_BUSNAME" == "x"],
    [SYNC_BUSNAME="xyz.openbmc_project.Software.Sync"])
AC_DEFINE_UNQUOTED([SYNC_BUSNAME], ["$SYNC_BUSNAME"], [The D-Bus busname to own])

AC_DEFINE([MAPPER_BUSNAME], ["xyz.openbmc_project.ObjectMapper"], [Object mapper bus name])
AC_DEFINE([MAPPER_INTERFACE], ["xyz.openbmc_project.ObjectMapper"], [Object mapper interface])
AC_DEFINE([MAPPER_PATH], ["/xyz/openbmc_project/object_mapper"], [Object mapper DBUS path])

AC_DEFINE(VERSION_IFACE, "xyz.openbmc_project.Software.Version",
    [The software version manager interface])
AC_DEFINE(FILEPATH_IFACE, "xyz.openbmc_project.Common.FilePath",
    [The common file path interface])
AC_DEFINE(OS_RELEASE_FILE, "/etc/os-release",
    [The name of the BMC table of contents file])
AC_DEFINE(MEDIA_DIR, "/media/",
    [The base dir where all RO partitions are mounted])
AC_DEFINE(BMC_ROFS_PREFIX, "/media/rofs-",
    [The prefix path for the versioned read-only bmc partitions])
AC_DEFINE(PERSIST_DIR, "/var/lib/obmc/phosphor-bmc-code-mgmt/",
    [The dir where activation data is stored in files])
AC_DEFINE(SYSTEMD_BUSNAME, "org.freedesktop.systemd1",
    [The systemd busname])
AC_DEFINE(SYSTEMD_PATH, "/org/freedesktop/systemd1",
    [The systemd path])
AC_DEFINE(SYSTEMD_INTERFACE, "org.freedesktop.systemd1.Manager",
    [The systemd interface])

AC_ARG_VAR(IMG_UPLOAD_DIR, [Directory where downloaded software images are placed])
AS_IF([test "x$IMG_UPLOAD_DIR" == "x"], [IMG_UPLOAD_DIR="/tmp/images"])
AC_DEFINE_UNQUOTED([IMG_UPLOAD_DIR], ["$IMG_UPLOAD_DIR"], [Directory where downloaded software images are placed])

AC_ARG_VAR(SYNC_LIST_FILE_NAME, [The name of the sync list file])
AS_IF([test "x$SYNC_LIST_FILE_NAME" == "x"], [SYNC_LIST_FILE_NAME="synclist"])
AC_DEFINE_UNQUOTED([SYNC_LIST_FILE_NAME], ["$SYNC_LIST_FILE_NAME"], [The name of the sync list file])

AC_ARG_VAR(SYNC_LIST_DIR_PATH, [The path to the sync list file directory])
AS_IF([test "x$SYNC_LIST_DIR_PATH" == "x"], [SYNC_LIST_DIR_PATH="/etc/"])
AC_DEFINE_UNQUOTED([SYNC_LIST_DIR_PATH], ["$SYNC_LIST_DIR_PATH"], [The path to the sync list file directory])

AC_ARG_VAR(MANIFEST_FILE_NAME, [The name of the MANIFEST file])
AS_IF([test "x$MANIFEST_FILE_NAME" == "x"], [MANIFEST_FILE_NAME="MANIFEST"])
AC_DEFINE_UNQUOTED([MANIFEST_FILE_NAME], ["$MANIFEST_FILE_NAME"], [The name of the MANIFEST file])

AC_ARG_VAR(PUBLICKEY_FILE_NAME, [The name of the public key file])
AS_IF([test "x$PUBLICKEY_FILE_NAME" == "x"], [PUBLICKEY_FILE_NAME="publickey"])
AC_DEFINE_UNQUOTED([PUBLICKEY_FILE_NAME], ["$PUBLICKEY_FILE_NAME"], [The name of the public key file])

AC_ARG_VAR(HASH_FILE_NAME, [Hash file name])
AS_IF([test "x$HASH_FILE_NAME" == "x"], [HASH_FILE_NAME="hashfunc"])
AC_DEFINE_UNQUOTED([HASH_FILE_NAME], ["$HASH_FILE_NAME"], [The name of the hash file])

AC_ARG_VAR(SIGNED_IMAGE_CONF_PATH, [Path of public key and hash function files])
AS_IF([test "x$SIGNED_IMAGE_CONF_PATH" == "x"], [SIGNED_IMAGE_CONF_PATH="/etc/activationdata/"])
AC_DEFINE_UNQUOTED([SIGNED_IMAGE_CONF_PATH], ["$SIGNED_IMAGE_CONF_PATH"], [Path of public key and hash function files])

AC_ARG_VAR(SIGNATURE_FILE_EXT, [The extension of the Signature file])
AS_IF([test "x$SIGNATURE_FILE_EXT" == "x"], [SIGNATURE_FILE_EXT=".sig"])
AC_DEFINE_UNQUOTED([SIGNATURE_FILE_EXT], ["$SIGNATURE_FILE_EXT"], [The extension of the Signature file])

AC_ARG_VAR(ACTIVE_BMC_MAX_ALLOWED, [The maximum allowed active BMC versions])
AS_IF([test "x$ACTIVE_BMC_MAX_ALLOWED" == "x"], [ACTIVE_BMC_MAX_ALLOWED=2])
AC_DEFINE_UNQUOTED([ACTIVE_BMC_MAX_ALLOWED], [$ACTIVE_BMC_MAX_ALLOWED],
    [The maximum allowed active BMC versions])

# setup signature verification
AC_ARG_ENABLE([verify_signature],
    AS_HELP_STRING([--enable-verify_signature], [Enable image signature validation.]))
AS_IF([test "x$enable_verify_signature" == "xyes"], \
    [AC_DEFINE([WANT_SIGNATURE_VERIFY],[],[Enable image signature validation.])])
AM_CONDITIONAL([WANT_SIGNATURE_VERIFY_BUILD], [test "x$enable_verify_signature" == "xyes"])

AC_DEFINE(BUSNAME_UPDATER, "xyz.openbmc_project.Software.BMC.Updater",
    [The item updater DBus busname to own.])

# setup sync of filesystem files
AC_ARG_ENABLE([sync_bmc_files],
    AS_HELP_STRING([--enable-sync_bmc_files], [Enable sync of filesystem files.]))
AS_IF([test "x$enable_sync_bmc_files" == "xyes"], \
    [AC_DEFINE([WANT_SYNC],[],[Enable sync of filesystem files.])])
AM_CONDITIONAL([WANT_SYNC], [test "x$enable_sync_bmc_files" == "xyes"])

# Check for header files.
AC_CHECK_HEADER(systemd/sd-bus.h, ,[AC_MSG_ERROR([Could not find systemd/sd-bus.h...systemd development package required])])
AC_CHECK_HEADER(sdbusplus/server.hpp, ,[AC_MSG_ERROR([Could not find sdbusplus/server.hpp...openbmc/sdbusplus package required])])

AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT
