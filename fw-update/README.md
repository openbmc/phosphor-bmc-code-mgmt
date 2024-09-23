# FW Update Daemons

These daemons are created following this design:
https://github.com/openbmc/docs/blob/master/designs/code-update.md

Each one inherits from the classes found in the common folder.

Device-specific class members can be added to implement the code update flow for
different devices.
