# SPI Device Update Daemon

This daemon is for updating SPI flash chips commonly used for Host Bios.

## Configuration Example 1 (Tyan S8030)

This is an example EM Exposes record which can appear on dbus as

```
xyz.openbmc_project.Configuration.SPIFlash
```

This config is untested.

```json
{
  "Name": "HostSPIFlash",
  "Path": "1e630000.spi",
  "HasME": false,
  "Layout": "Flat",
  "MuxGpios": [
    {
      "Name": "BMC_SPI_SEL",
      "Polarity": "High"
    }
  ],
  "VendorIANA": "6653",
  "Compatible": "com.tyan.Hardware.S8030.SPI.Host",
  "Type": "SPIFlash"
}
```

- 'HasME' is referring to the Intel Management Engine.

## Configuration Example 2 (Tyan S5549)

Here we are writing a previously dumped flash image using flashrom.

```json
{
  "Name": "HostSPIFlash",
  "Path": "1e630000.spi",
  "HasME": true,
  "Layout": "Flat",
  "Tool": "flashrom",
  "MuxGpios": [
    {
      "Name": "FM_BIOS_SPI_SWITCH",
      "Polarity": "High"
    },
    {
      "Name": "FM_BMC_FLASH_SEC_OVRD_N",
      "Polarity": "High"
    }
  ],
  "VendorIANA": "6653",
  "Compatible": "com.tyan.Hardware.S5549.SPI.Host",
  "Type": "SPIFlash"
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
