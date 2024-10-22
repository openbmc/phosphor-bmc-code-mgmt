# Voltage Regulator Update Daemon

This daemon implements the update process for voltage regulators attached via
I2C/PMBus bus.

## Configuration Example

This example shows an addition to configuration for the Infineon XDPE1X2XX target. It
only shows the DBUS related information provided by the configuration.

### EntityManager configuration

```json
{
  "Address": "0x66",
  "Bus": 28,
  "Name": "MB_VR_CPU_VCORE1_PDDIO",
  "Type": "XDPE1X2XX",
  "FirmwareInfo": {
    "VendorIANA": "6653",
    "CompatibleHardware": "com.tyan.Hardware.S8030.SPI.Host"
  },
}
```
