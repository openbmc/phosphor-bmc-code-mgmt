# OCP Secure Firmware Recovery updater

This daemon (`phosphor-ocp-recovery-software-update`) recovers devices that
implement the OCP Security WG
[Secure Firmware Recovery](https://www.opencompute.org/documents/ocp-recovery-document-1p0-final-1-pdf)
protocol over I2C/SMBus. It pushes a recovery image into the device's
component memory space (CMS) via the v1.0 indirect-write register interface
and activates it — the path of last resort when a device is bricked or
parked in its recovery ROM and no longer reachable through its normal
firmware update transport (e.g. PLDM over MCTP).

It is built on the [common firmware update framework](../common/README.md)
and the pure-C protocol library [`libocp`](../libocp), enabled with the
meson option `ocp-recovery-software-update`.

## Protocol summary

The recovery agent (this daemon) talks to the device with SMBus block
transactions (`[command, byte count, payload...]`). The sequence per update:

1. `PROT_CAP` (0x22, tolerated if unimplemented): check magic `"OCP RECV"`
   and the capability bits. Devices advertising only the v1.1
   `INDIRECT_FIFO` mechanism are rejected (not implemented yet).
2. `DEVICE_STATUS` (0x24): if the device is not in recovery mode (0x3),
   force it there with `DEVICE_RESET` (0x25, payload `01 0F 01`) and poll
   until it arrives (configurable timeout).
3. `RECOVERY_CTRL` (0x26): select the CMS and "recover from memory window".
4. `INDIRECT_CTRL` (0x29): select the CMS write window at offset 0.
5. `INDIRECT_DATA` (0x2B): stream the image in chunks (default 252 bytes),
   polling the acknowledge bit in `INDIRECT_STATUS` (0x2A) after each chunk.
6. `RECOVERY_CTRL` activate (`0x0F`), then poll `RECOVERY_STATUS` (0x27)
   until it reports recovery success (0x3) or an error (0xC-0xF).

## Entity Manager configuration

Devices are discovered from an `OCPRecoveryFirmware` exposes record. The
`FirmwareInfo` section is required by the common framework and is matched
against the PLDM package's descriptors:

```json
{
    "Name": "MyASICRecovery",
    "Type": "OCPRecoveryFirmware",
    "Bus": "$bus",
    "Address": "0x6f",
    "FirmwareInfo": {
        "VendorIANA": 5703,
        "CompatibleHardware": "com.example.Hardware.MyBoard.ASIC"
    }
}
```

Optional properties:

| Property                      | Default | Meaning                                        |
| ----------------------------- | ------- | ---------------------------------------------- |
| `ChunkSize`                   | 252     | INDIRECT_DATA payload bytes per block write (use 32 behind CP2112-class USB-I2C bridges) |
| `MemoryWindow`                | 0       | CMS index the image is written to              |
| `ForceRecoveryTimeoutSeconds` | 30      | How long to wait for the device to enter recovery mode after a forced reset |

## Usage

The daemon claims `xyz.openbmc_project.Software.OCPRecovery` and exposes,
per device, a software object under `/xyz/openbmc_project/software/` with
`xyz.openbmc_project.Software.Version` and `xyz.openbmc_project.Software.Update`.
The image must be a PLDM (DSP0267) firmware update package whose component
matches the configured `VendorIANA`/`CompatibleHardware`. Only the
`Immediate` apply time is supported — activation restarts the device into
the recovered image.

D-Bus:

```
busctl call xyz.openbmc_project.Software.OCPRecovery \
    /xyz/openbmc_project/software/<swid> \
    xyz.openbmc_project.Software.Update StartUpdate hs \
    <package fd> xyz.openbmc_project.Software.ApplyTime.RequestedApplyTimes.Immediate
```

The call returns a new software object path carrying
`Software.Activation` (Ready → Activating → Active/Failed) and
`Software.ActivationProgress`.

Redfish (multipart push targeting this device's firmware inventory entry):

```
curl -k -u root:0penBmc \
    -F 'UpdateParameters={"Targets":["/redfish/v1/UpdateService/FirmwareInventory/<swid>"],"@Redfish.OperationApplyTime":"Immediate"};type=application/json' \
    -F 'UpdateFile=@recovery-package.pldm;type=application/octet-stream' \
    https://${bmc}/redfish/v1/UpdateService/update-multipart
```

The returned Task tracks the activation progress reported by the daemon.
