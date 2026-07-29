#pragma once

#include <libusb-1.0/libusb.h>

#include <gpiod.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace phosphor::software::usb_rcm
{

// GPIO strap layout used to drive a device into USB RCM recovery mode. Selects
// the board count and the GPIO line-name prefixes (see usb_rcm_driver.cpp).
enum class ConfigType
{
    c2,   // single board, prefix B0_M1_CPU / B0_M1
    c1g2, // single board, prefix BRD0_CPU / BRD0
    c2g4, // two boards,   prefix BRD<n>_CPU / BRD<n>
};

std::optional<ConfigType> parseConfigType(const std::string& value);

// One USB RCM recovery target (an NVIDIA Vera CPU in RCM mode).
struct DeviceConfig
{
    // Device name, e.g. "CPU_0" — its trailing index selects the DOT blob
    // (<dotBlobDir>/CPU_<n>.bin) and labels log output.
    std::string name;

    // Linux USB port path of the RCM device, e.g. "1-9" or "1-1.3.4".
    std::string usbPort;

    // GPIO strap layout for force-recovery entry.
    ConfigType configType = ConfigType::c2;

    // Directory holding the per-CPU DOT blobs (device-specific, out of band).
    std::string dotBlobDir = "/var/emmc/misc/dot-blob";

    // RCM bootrom USB identity.
    uint16_t vendorId = 0x0955;
    uint16_t productId = 0x7410;

    // RCM recovery USB interface. Interface 3 is stable across the Vera bootrom
    // revisions seen so far; overridable via the USBRCMFirmware entity-manager
    // record for future parts.
    int recoveryInterface = 3;

    // RCM bulk-OUT endpoint on the recovery interface. 0 (the default) means
    // auto-discover it from the interface descriptor, which is correct for
    // every revision (the reference exposes it at 0x08, some parts at 0x0a).
    // Set a non-zero value in the USBRCMFirmware record only to pin a specific
    // endpoint number.
    uint8_t recoveryEndpoint = 0;
};

enum class RecoveryState
{
    inRecovery,       // present in RCM mode (recovery interface + bulk-OUT ep)
    recoveryComplete, // boot progress-code queue reports PLDM T5 ready
    notInRecovery,    // reachable but not in RCM mode
    unreachable,      // no matching device on the configured port
};

// One in-memory blob to stream to the device (a firmware component image, in
// RCM flash order). The DOT blob is read from disk by the driver, not here.
struct Image
{
    const uint8_t* data = nullptr;
    size_t size = 0;
    std::string label;
};

struct RecoveryResult
{
    bool success = false;
    // Soft success: an empty (all-zero) DOT blob was accepted by the PSC ROM.
    bool emptyDotAccepted = false;
    std::string error;
    int errorCode = 0; // USBRCMRecoveryErrorCode value (0 on success)
};

const char* toString(RecoveryState state);

// Drives the NVIDIA Vera-CPU USB Recovery-Mode protocol for one device:
// GPIO force-recovery entry, ECID-gated DOT-blob selection, bulk image
// streaming over libusb, and boot progress-code polling for success.
//
// Synchronous and blocking (bulk transfers and settle waits run inline), so a
// caller on an event loop must run recover()/forceRecovery() off the reactor
// thread.
class RecoveryDriver
{
  public:
    RecoveryDriver(const RecoveryDriver&) = delete;
    RecoveryDriver& operator=(const RecoveryDriver&) = delete;
    RecoveryDriver(RecoveryDriver&&) = delete;
    RecoveryDriver& operator=(RecoveryDriver&&) = delete;

    explicit RecoveryDriver(DeviceConfig config);
    ~RecoveryDriver();

    // Classify the recovery state of the configured device.
    RecoveryState status();

    // Drive the force-recovery GPIO straps (phased reset -> straps -> release).
    bool forceRecovery();

    // Restore the default cold-boot strap states after a recovery.
    bool clearForceRecovery();

    // Stream the ordered images to the device over the RCM bulk endpoint. Reads
    // the ECID to decide whether a DOT blob is required and, if so, prepends
    // the per-CPU blob from dotBlobDir. `images` must already be in RCM flash
    // order.
    RecoveryResult recover(const std::vector<Image>& images,
                           bool verbose = false);

    const DeviceConfig& config() const
    {
        return cfg;
    }
    const std::string& lastError() const
    {
        return lastErr;
    }

  private:
    enum class BlobRequirement
    {
        none,
        dot,
        s2a,
    };

    // Outcome of polling the boot progress-code queue after an image transfer.
    struct ProgressOutcome
    {
        bool complete = false;            // saw PLDM T5 ready
        bool fatalError = false;          // saw a non-ignorable error code
        bool mutableDotSoftError = false; // saw an ignorable mutable-DOT error
        uint32_t lastCode = 0;
    };

    bool ensureUsb();
    // Finds the configured device (by port path). Returns a referenced device
    // (caller must libusb_unref_device); nullptr if not present.
    libusb_device* findDevice();

    static std::optional<uint32_t> readRegister32(libusb_device_handle* handle,
                                                  uint32_t address);
    static std::optional<std::array<uint8_t, 32>> readEcid(
        libusb_device* device, libusb_device_handle* handle);
    static BlobRequirement blobRequirement(const std::array<uint8_t, 32>& ecid);

    static bool sendBulkData(libusb_device_handle* handle, const uint8_t* data,
                             size_t size, uint8_t recoveryEndpoint,
                             int& libusbError);
    static ProgressOutcome monitorProgress(libusb_device_handle* handle);

    std::string dotBlobPath() const;

    // Force-recovery GPIO helpers.
    bool driveStraps(bool assertRecovery);

    DeviceConfig cfg;
    std::string lastErr;
    libusb_context* usbCtx = nullptr;
};

} // namespace phosphor::software::usb_rcm
