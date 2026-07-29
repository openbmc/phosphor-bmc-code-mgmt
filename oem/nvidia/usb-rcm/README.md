# NVIDIA USB RCM firmware recovery

`phosphor-usb-rcm-recovery-software-update` recovers the firmware of an NVIDIA
Vera CPU through the standard Redfish / `xyz.openbmc_project.Software.Update`
firmware-update path.

A Vera CPU that fails to boot drops into its bootrom **USB Recovery Mode
(RCM)**, enumerating as `VID:PID 0955:7410`. In that state its normal firmware
transport is gone, so it must be re-flashed out of band. This daemon (and the
`nvidia-usb-rcm-recovery-tool` CLI) implement that path of last resort.

This code lives under `oem/nvidia/` and only builds when the `oem-nvidia` meson
feature is enabled. The entire RCM protocol is implemented **in-code over
libusb** — there is no external flashing tool — so it is upstreamable to LF
OpenBMC.

## Mechanism

Recovery of one Vera CPU is:

1. If the device is not already in RCM mode, drive it there over the
   force-recovery **GPIO straps** (assert `FORCED_RECOVERY_L`, select
   `RECOVERY_TYPE = USB RCM`, and pulse reset). The strap GPIO line names depend
   on the board layout (`ConfigType` = `c2`, `c1g2` or `c2g4`).
2. Read the device **ECID** (USB serial descriptor) to decide whether a
   device-specific **DOT blob** is required. S2A-blob parts are not supported.
3. Stream the firmware component images (and the DOT blob first, when required)
   to the recovery interface's **bulk-OUT endpoint** in 512-byte chunks, in RCM
   flash order. The endpoint is auto-discovered from the interface descriptor
   (`0x08` on the reference bootrom, `0x0a` on some revisions).
4. Poll the boot **progress-code queue** for the PLDM-T5-ready completion code.
   The 1024-byte empty-DOT-blob soft-success case is preserved.

The USB match/open, ECID read, bulk streaming and progress polling all use
libusb; the straps use libgpiodcxx. All of this lives in the transport-agnostic
`RecoveryDriver` (`usb_rcm_driver.{hpp,cpp}`), shared by the CLI tool and the
daemon.

## Update package

The framework drives updates from a PLDM (DSP0267) package, so the Vera firmware
components must be wrapped in a PLDM package whose `FirmwareInfo` matches the
device's entity-manager config (`VendorIANA` + `CompatibleHardware`). The daemon
consumes every applicable component and streams them **in package order**, which
must therefore be the RCM flash order (e.g. `SBIOS_FMC`, `L1_PT`, `CSH-OEM_FWS`,
`Caliptra_FMC`, `PSC_BCT`, `CSH-NV_FWS`, `OOBHUB_FW`, `PSC_FW_Text`,
`PSC_FW_Data`). The DOT blob is sourced out of band from
`<DotBlobDir>/CPU_<n>.bin` (default `/var/emmc/misc/dot-blob`), keyed by the
device name's trailing index.

## Entity-manager configuration

Devices are discovered from `xyz.openbmc_project.Configuration.USBRCMFirmware`
exposes records. One record describes one Vera CPU and produces one
`Software.Update` object.

```json
{
  "Name": "CPU_0",
  "Type": "USBRCMFirmware",
  "USBPort": "1-9",
  "ConfigType": "c2",
  "FirmwareInfo": {
    "VendorIANA": 5703,
    "CompatibleHardware": "com.nvidia.Hardware.Vera"
  }
}
```

`ConfigType` defaults to `c2`; `DotBlobDir` defaults to
`/var/emmc/misc/dot-blob`; `VendorId`/`ProductId` default to `0x0955`/`0x7410`.
`RecoveryInterface` defaults to `3`. `RecoveryEndpoint` defaults to `0`, meaning
the recovery interface's bulk-OUT endpoint is auto-discovered from the USB
descriptor; set it (decimal, e.g. `10` for `0x0a`) only to pin a specific
endpoint number on a revision the discovery cannot handle.

## D-Bus / Redfish usage

The daemon owns `xyz.openbmc_project.Software.USBRCMRecovery` and, for each
configured CPU, publishes a `/xyz/openbmc_project/software/<swid>` object with
`Software.Version` (`Version=unknown`, `Purpose=Other` until a recovery
succeeds) and `Software.Update` (`AllowedApplyTimes=[Immediate]`). Trigger a
recovery with a targeted Redfish multipart update against that object's
`FirmwareInventory` entry, or via `Software.Update.StartUpdate` with a PLDM
package fd. The blocking recovery runs on a worker thread so the daemon stays
responsive.

## CLI bring-up tool

`nvidia-usb-rcm-recovery-tool` drives the same driver directly for bench work:

```sh
nvidia-usb-rcm-recovery-tool --usb-port 1-9 GetRecoveryStatus
nvidia-usb-rcm-recovery-tool --usb-port 1-9 --config-type c2 SetForceRecovery
nvidia-usb-rcm-recovery-tool --usb-port 1-9 --name CPU_0 \
    PerformUSBRecovery -i SBIOS_FMC.bin L1_PT.bin ... PSC_FW_Data.bin
nvidia-usb-rcm-recovery-tool --usb-port 1-9 --config-type c2 ClearForceRecovery
```
