# SPI Device Update Daemon

This daemon is for updating SPI flash chips commonly used for Host Bios.

## Configuration Example 1 (Tyan S8030)

This is an example EM Exposes record which can appear on dbus as

```
xyz.openbmc_project.Configuration.BIOS
```

This config is untested.

```json
{
  "Name": "HostSPIFlash",
  "SPIDevicePath": "1e630000.spi",
  "HasME": false,
  "Layout": "Flat",
  "MuxOutputs": ["BMC_SPI_SEL"],
  "MuxGPIOValues": [1],
  "VendorIANA": "6653",
  "Compatible": "com.tyan.Hardware.S8030.SPI.Host",
  "Type": "BIOS"
}
```

- 'HasME' is referring to the Intel Management Engine.

## Configuration Example 2 (Tyan S5549)

Here we are writing a previously dumped flash image using flashrom.

```json
{
  "Name": "HostSPIFlash",
  "SPIDevicePath": "1e630000.spi",
  "HasME": true,
  "Layout": "Flat",
  "Tool": "flashrom",
  "MuxOutputs": ["FM_BIOS_SPI_SWITCH", "FM_BMC_FLASH_SEC_OVRD_N"],
  "MuxGPIOValues": [1, 1],
  "VendorIANA": "6653",
  "Compatible": "com.tyan.Hardware.S5549.SPI.Host",
  "Type": "BIOS"
}
```

## Layout information

Sometimes another tool is needed if one does not have a flat image.
Configuration fragments below.

No tool, flat image. This can be used for example when we want to write a flash
image which was previously dumped.

```json
{
  "Layout": "Flat"
}
```

Use flashrom to write with information from an intel flash descriptor

```json
{
  "Layout": "IFD"
}
```

## Tool information

We can directly write to the mtd device or use flashrom to do the writing. In
case you want to use Intel Flash Descriptor (not flat layout) or do additional
verification, flashrom can be used.

```json
{
  "Tool": "flashrom"
}
```

```json
{
  "Tool": "None"
}
```
