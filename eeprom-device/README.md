# EEPROM Device Update Daemon

This daemon implements the update process for EEPROM device attached via I2C.

## Entity Manager Configuration Example (Harma)

The following JSON snippet demonstrates how to configure an EEPROM device,
including bus details, address, GPIO settings, and compatibility information.

```json
{
  "Name": "MB_Retimer",
  "Bus": 20,
  "Address": "0x50",
  "MuxGpios": [
    {
      "Name": "rt-cpu0-p1-enable",
      "Polarity": "High",
      "ChipName": "gpiochip2"
    },
    {
      "Name": "smb-rt-rom-p1-select",
      "Polarity": "High",
      "ChipName": "gpiochip2"
    }
  ],
  "VendorIANA": 40981,
  "Compatible": "com.harma.Hardware.pt5161l.Retimer",
  "Type": "EEPROMDeviceFirmware"
}
```

## Entity Manager Interface

The configured EEPROM device will appear under the D-Bus path as shown below:

```bash
/xyz/openbmc_project/inventory/system/board/Harma_MB/MB_Retimer
```

The EEPROM device is associated with the following D-Bus interface:

```bash
xyz.openbmc_project.Configuration.EEPROM
```
