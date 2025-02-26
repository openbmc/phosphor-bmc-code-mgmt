# Voltage Regulator Update Daemon

This daemon implements the update process for voltage regulators attached via
I2C/PMBus bus.

## Configuration Example

This example shows an addition to configuration for the lattice LCMXO3LF-4300C
target. It only shows the DBUS related information provided by the
configuration.

### EntityManager configuration

```json
{
  "Name": "CPLD_LCMXO3LF_4300C",
  "Address": "0x40",
  "Bus": 5,
  "VendorIANA": 40981,
  "Compatible": "com.meta.Hardware.CPLD",
  "ChipVendor": "lattice",
  "ChipName": "LCMXO3LF-4300",
  "Type": "CPLDFirmware"
}
```
