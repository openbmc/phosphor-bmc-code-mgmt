# SPI Device Update Daemon

This daemon is for updating SPI flash chips. Commonly used for Host Bios, but
also for some on-board controllers.

## Configuration Example 1 (host BIOS on Tyan S8030)

This is an example EM Exposes record which can appear on dbus as

```text
xyz.openbmc_project.Configuration.SPIFlash
```

```json
{
  "Name": "HostSPIFlash",
  "SPIControllerIndex": "1",
  "SPIDeviceIndex": "0",
  "MuxOutputs": [
    {
      "Name": "BMC_SPI_SEL",
      "Polarity": "High"
    }
  ],
  "VendorIANA": "6653",
  "Layout": "Flat",
  "FirmwareInfo": {
    "VendorIANA": "6653",
    "CompatibleHardware": "com.tyan.Hardware.S8030.SPI.Host"
  },
  "Type": "HostSPIFlash"
}
```

## Configuration example 2 (BCM51358 Ethenet switch)

This is an example EM Exposes record which can appear on dbus as

```text
xyz.openbmc_project.Configuration.BCM51358Firmware
```

```json
{
  "Name": "BCM_Network",
  "SPIControllerIndex": 1,
  "SPIDeviceIndex": 0,
  "SerialPort": "/dev/ttyS1",
  "SerialBaudRate": 9600,
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
    "VendorIANA": 7154,
    "CompatibleHardware": "tech.design.Hardware.bcm51358"
  },
  "Type": "BCM51358Firmware"
}
```

## Layout information

Sometimes another tool is needed if one does not have a flat image. Use "Layout"
property to give that hint. Possible values:

- "Flat" : No tool, flat image. This can be used for example when we want to
  write a flash image which was previously dumped.

## Tool information

We can directly write to the mtd device or use flashrom to do the writing.
