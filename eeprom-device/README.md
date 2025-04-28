# EEPROM Device Update Daemon

This daemon implements the update process for EEPROM device attached via I2C.

## Entity Manager Configuration Example (Harma)

The following JSON snippet demonstrates how to configure an EEPROM device,
including bus details, address, GPIO settings, and compatibility information.

```json
{
  "Name": "MB_Retimer",
  "Bus": 12,
  "Address": "0x24",
  "FirmwareDevice": "MB_Retimer_EEPROM",
  "MuxOutputs": [
    {
      "Name": "rt-cpu0-p1-enable",
      "Polarity": "High"
    },
    {
      "Name": "smb-rt-rom-p1-select",
      "Polarity": "High"
    }
  ],
  "FirmwareInfo": {
    "VendorIANA": 40981,
    "CompatibleHardware": "com.meta.Hardware.Harma.pt5161l.Retimer"
  },
  "Type": "PT5161L"
}
```

## Entity Manager Interface

The EEPROM device configuration can be found at the following D-Bus path,
provided that the EEPROMDevice is a expose record in the Harma_MB Entity Manager
configuration:

```bash
/xyz/openbmc_project/inventory/system/board/Harma_MB/MB_Retimer
```

The D-Bus interface name for EEPROMDevice configuration will be as follows:

```bash
xyz.openbmc_project.Configuration.EEPROMDevice
```
