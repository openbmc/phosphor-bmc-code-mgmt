# SPI Device Update Daemon

This daemon is for updating SPI flash chips commonly used for Host Bios.

## Configuration Example 1 (Tyan S8030)

This is an example EM Exposes record which can appear on dbus as

```
xyz.openbmc_project.Configuration.SPIFlash
```

or

```
xyz.openbmc_project.Configuration.IntelSPIFlash
```

```json
{
  "Name": "HostSPIFlash",
  "SPIControllerIndex": "1",
  "SPIDeviceIndex": "0",
  "HasME": false,
  "MuxOutputs": ["BMC_SPI_SEL"],
  "MuxGPIOValues": [1],
  "VendorIANA": "6653",
  "Layout": "Flat",
  "Tool": "flashrom",
  "FirmwareInfo": {
    "VendorIANA": "6653",
    "CompatibleHardware": "com.tyan.Hardware.S8030.SPI.Host"
  },
  "Type": "SPIFlash"
}
```

- 'HasME' is referring to the Intel Management Engine.

## Layout information

Sometimes another tool is needed if one does not have a flat image. Use "Layout"
property to give that hint. Possible values:

- "Flat" : No tool, flat image. This can be used for example when we want to
  write a flash image which was previously dumped.

- "IntelFlashDescriptor": Use flashrom to write with information from an intel
  flash descriptor

## Tool information

We can directly write to the mtd device or use flashrom to do the writing. In
case you want to use Intel Flash Descriptor (not flat layout) or do additional
verification, flashrom can be used.
