# CPLD Update Daemon

This daemon implements the update process for CPLD attached via I2C bus.

## Configuration Example

The following shows an example of entity manager configuration record for
lattice LCMXO3LF-4300C CPLD.

```json
{
  "Address": "0x40",
  "Bus": 5,
  "FirmwareInfo": {
      "CompatibleHardware": "com.meta.Hardware.Harma.CPLD.LCMXO3LF_4300C_mb",
      "VendorIANA": 40981
  },
  "Name": "Harma_MB_CPLD",
  "Type": "LatticeMachXO3Firmware"
}
```
