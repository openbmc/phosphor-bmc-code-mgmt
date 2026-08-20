# EEPROM Device Update Daemon

This daemon implements the update process for EEPROM device attached via I2C.

## Entity Manager Configuration Examples

The following JSON snippets demonstrate how to configure an EEPROM device,
including bus details, address, GPIO settings, and compatibility information.

### PT5161L Retimer with I2C EEPROM (Harma)

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
  "Type": "PT5161LFirmware"
}
```

### BCM51358 Ethenet switch over serial port + SPI EEPROM

```json
        {
            "SPIControllerIndex": 1,
            "SPIDeviceIndex": 0,
            "Name": "BCM_EEPROM",
            "Type": "DeviceSPIFlash"
        },
        {
            "Name": "BCM_Network",
            "Port": "/dev/ttyS1",
            "Baudrate": 9600,
            "FirmwareDevice": "BCM_EEPROM",
            "MuxOutputs": [
                {
                    "Name": "BCM_ROM_SEL",
                    "Polarity": "High"
                }
            ],
            "ResetOutputs": [
                {
                    "Name": "BCM1_RST",
                    "Polarity": "Low"
                }
            ],
            "FirmwareInfo": {
                "VendorIANA": 4413,
                "CompatibleHardware": "tech.design.Hardware.bcm51358"
            },
            "Type": "BCM51358Firmware"
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
