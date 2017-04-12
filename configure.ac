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
PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging],,
    [AC_MSG_ERROR([The openbmc/phosphor-logging package is required])])

# Checks for library functions
LT_INIT # Required for systemd linking

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext])
AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

AC_ARG_VAR(VERSION_BUSNAME, [The Dbus busname to own])
AS_IF([test "x$VERSION_BUSNAME" == "x"], [VERSION_BUSNAME="xyz.openbmc_project.Software.BMC.Version"])
AC_DEFINE_UNQUOTED([VERSION_BUSNAME], ["$VERSION_BUSNAME"], [The DBus busname to own])

AC_ARG_VAR(SOFTWARE_OBJPATH, [The version software manager Dbus root])
AS_IF([test "x$SOFTWARE_OBJPATH" == "x"], [SOFTWARE_OBJPATH="/xyz/openbmc_project/software"])
AC_DEFINE_UNQUOTED([SOFTWARE_OBJPATH], ["$SOFTWARE_OBJPATH"], [The version software manager Dbus root])

# Check for header files.
AC_CHECK_HEADER(systemd/sd-bus.h, ,[AC_MSG_ERROR([Could not find systemd/sd-bus.h...systemd developement package required])])
AC_CHECK_HEADER(sdbusplus/server.hpp, ,[AC_MSG_ERROR([Could not find sdbusplus/server.hpp...openbmc/sdbusplus package required])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
