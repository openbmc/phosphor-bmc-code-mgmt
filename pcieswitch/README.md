# PCIe switch Update Daemon

This daemon is for updating SPI flash for PCIe switch.

## Configuration Example

This is an example EM Exposes record which can appear on dbus as

```text
xyz.openbmc_project.Configuration.BRCM_PEX90080Firmware
```

```json
{
    "Address": "0xFF",
    "Bus": 0,
    "FirmwareInfo": {
        "CompatibleHardware": "com.meta.Hardware.Anacapa.PCIESW.PEX90080_rbb",
        "VendorIANA": 40981
    },
    "MuxOutputs": [
        {
            "Name": "RBB_SPI_MUX0_R2_SEL",
            "Polarity": "High"
        }
    ],
    "Name": "Anacapa_RBB_PCIESWITCH",
    "Type": "BRCM_PEX90080Firmware"
}
```
