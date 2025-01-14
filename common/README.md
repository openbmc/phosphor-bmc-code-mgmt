# Common Firmware Library

This library follows the code update design:
https://github.com/openbmc/docs/blob/master/designs/code-update.md

It enables implementing code updaters for different devices. Each one inherits
from the classes found in the common folder.

Device-specific class members can be added to implement the code update flow for
different devices.

## PLDM Package Parser

The PackageParser in the pldm directory currently references a following
revision from the openbmc/pldm repository -

https://github.com/openbmc/pldm/blob/f48015b39f945c2f9534e674819bdfef7b6c7054/fw-update/package_parser.cpp#L294

However, this code will be deprecated and replaced with the package parsing APIs
provided by libpldm once they become available.

The PackageParser has a maximum supported PLDM firmware package revision of
1.0.0.
