# Voltage Regulator Update Daemon

This daemon implements the update process for voltage regulators attached via
I2C bus.

## Configuration Example

This example shows an addition to configuration for the Yosemitev2 target. It
only shows the DBUS related information provided by the configuration and DOES
NOT represent any hardware available on the real platform.

### EntityManager configuration

```json
{
            "Address": "0x66",
            "Bus": 28,
            "Labels": [...],
            "Name": "MB_VR_CPU_VCORE1_PDDIO",
            "PowerState": "On",
            "Thresholds": [...],
            "Type": "XDPE152C4",
			"Compatible": "com.meta.Hardware.Yosemitev2.XDPE152C4",
			"VendorIANA": 2345
        }
```

### Entity Manager Interface

/xyz/openbmc_project/inventory/system/board/Yosemite_V2_Baseboard/MB_VR_CPU_VCORE1_PDDIO

```
xyz.openbmc_project.Configuration.XDPE152C4
```