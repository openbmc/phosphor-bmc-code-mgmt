# EEPROM Update Daemon

This daemon implements the update process for EEPROM attached via I2C bus.

## Configuration Example

This example shows an addition to configuration for the Harma target. It
only shows the DBUS related information provided by the configuration and DOES
NOT represent any hardware available on the real platform.

### Entity Manager Configuration

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
	"Type": "EEPROM"
}
```

### Entity Manager Interface

/xyz/openbmc_project/inventory/system/board/Harma_MB/MB_Retimer

```bash
xyz.openbmc_project.Configuration.EEPROM
```