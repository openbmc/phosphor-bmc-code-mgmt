# FW Update Daemons

These daemons are created following this design:
https://github.com/openbmc/docs/blob/master/designs/code-update.md

Each one inherits from the classes found in the common folder.

Device-specific class members can be added to implement the code update flow for
different devices.

## PLDM FW Update Package

The Update Package should be revision 1.0.0 to be able to use the exisiting code
in the pldm repository to parse the package.

The existing code refers to this revision.

https://github.com/openbmc/pldm/blob/f48015b39f945c2f9534e674819bdfef7b6c7054/fw-update/package_parser.cpp#L294
