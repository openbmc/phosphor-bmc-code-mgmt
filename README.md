# phosphor-bmc-code-mgmt
Phosphor BMC Code Management provides a set of system software management
applications. More information can be found at
[Software Architecture](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/xyz/openbmc_project/Software/README.md)

## To Build
```
To build this package, do the following steps:

    1. ./bootstrap.sh
    2. ./configure ${CONFIGURE_FLAGS} --prefix=/usr
    3. make

To full clean the repository again run `./bootstrap.sh clean`.
```

```
For local build, pass --prefix=/usr option to the configure script
to let the Makefile use /usr/share value over /usr/local/share for ${datadir}
variable. The error yaml files and elog parser are stored in /usr/share
location in the SDK.
```
