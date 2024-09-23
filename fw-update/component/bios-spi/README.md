# SPI Device Update Daemon

This daemon is for updating SPI flash chips commonly used for Host Bios.

## Configuration

This is an example EM Exposes record which can appear on dbus as

```
xyz.openbmc_project.Configuration.SPIFlash
```

```json
{
  "Name": "HostSPIFlash",
  "Path": "1e630000.spi",
  "MuxGpios": [
    {
      "Name": "BMC_SPI_SEL",
      "Polarity": "High"
    }
  ],
  "Type": "SPIFlash"
}
```
