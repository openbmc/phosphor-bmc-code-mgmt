# SPI Device Update Daemon

This daemon is for updating SPI flash chips commonly used for Host Bios.

## Configuration Example 1 (Tyan S8030)

This is an example EM Exposes record which can appear on dbus as

```
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
  "Type": "SPIFlash"
}
```

## Layout information

Sometimes another tool is needed if one does not have a flat image. Use "Layout"
property to give that hint. Possible values:

- "Flat" : No tool, flat image. This can be used for example when we want to
  write a flash image which was previously dumped.

## Tool information

We can directly write to the mtd device or use flashrom to do the writing.
