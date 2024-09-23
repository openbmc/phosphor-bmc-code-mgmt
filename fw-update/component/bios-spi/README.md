# SPI Device Update Daemon

This daemon is for updating SPI flash chips commonly used for Host Bios.

## Configuration Example

This is an example EM Exposes record which can appear on dbus as

```
xyz.openbmc_project.Configuration.SPIFlash
```

```json
{
  "Name": "HostSPIFlash",
  "Path": "1e630000.spi",
  "HasME": false,
  "MuxGpios": [
    {
      "Name": "BMC_SPI_SEL",
      "Polarity": "High"
    }
  ],
  "Type": "SPIFlash"
}
```

- 'HasME' is referring to the Intel Management Engine.

## Configuration Example 2

```json
{
  "Name": "HostSPIFlash",
  "Path": "1e630000.spi",
  "HasME": true,
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
  "Type": "SPIFlash"
}
```
