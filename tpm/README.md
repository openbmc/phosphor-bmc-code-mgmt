# TPM Update Daemon

This daemon handles firmware version retrieval and firmware update processes for
TPM devices. Currently, it supports reading the firmware version of both
Infineon and Nuvoton TPM 2.0 chips. Firmware update support will be added in a
future patch.

## Entity Manager Configuration Example

The snippet below demonstrates how to configure a TPM device in Entity Manager.

```json
{
  "FirmwareInfo": {
    "CompatibleHardware": "com.meta.Hardware.Yosemite5.TPM",
    "VendorIANA": 40981
  },
  "Name": "Yosemite5_TPM",
  "Type": "TPM2Firmware"
}
```

## Entity Manager Interface

The D-Bus interface name for TPM configuration will be as follows:

```bash
xyz.openbmc_project.Configuration.TPM2Firmware
```
