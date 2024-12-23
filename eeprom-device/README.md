# EEPROM Device Update Daemon

This daemon implements the update process for EEPROM device attached via I2C.

## Entity Manager Configuration Example (Harma)

The following JSON snippet demonstrates how to configure an EEPROM device,
including bus details, address, GPIO settings, and compatibility information.

```json
{
  "Name": "MB_Retimer",
  "DeviceInfo": {
    "Bus": 12,
    "Address": "0x24",
    "ChipModel": "pt5161l"
  },
  "EEPROMInfo": {
    "Bus": 20,
    "Address": "0x50",
    "ChipModel": "at24"
  },
  "MuxGpios": ["rt-cpu0-p1-enable", "smb-rt-rom-p1-select"],
  "MuxGPIOPolarities": ["ActiveHigh", "ActiveHigh"],
  "FirmwareInfo": {
    "VendorIANA": 40981,
    "CompatibleHardware": "com.harma.Hardware.pt5161l.Retimer"
  },
  "Type": "EEPROMDeviceFirmware"
}
```

## Entity Manager Interface

The EEPROM device configuration can be found at the following D-Bus path,
provided that the EEPROMDeviceFirmware is a expose record in the Harma_MB Entity
Manager configuration:

```bash
/xyz/openbmc_project/inventory/system/board/Harma_MB/MB_Retimer
```

The D-Bus interface name for EEPROMDeviceFirmware configuration will be as
follows:

```bash
xyz.openbmc_project.Configuration.EEPROMDeviceFirmware
```
