// SPDX-License-Identifier: Apache-2.0

/*
 * ocp_fw_recovery - OCP Security WG "Secure Firmware Recovery" protocol
 * (v1.0), indirect (CMS memory window) image push over I2C/SMBus block
 * transfers.
 *
 * Spec:
 * https://www.opencompute.org/documents/ocp-recovery-document-1p0-final-1-pdf
 *
 * Each SMBus transaction is:
 *   write: [command, byte count, payload ...]      (single i2c_msg)
 *   read : [command] write + I2C_M_RD read message (one I2C_RDWR ioctl);
 *          the response starts with the SMBus block byte count.
 *
 * Every fallible operation returns std::expected<..., std::error_code>.
 * Transport failures carry the underlying errno; malformed device
 * responses map to std::errc::protocol_error; a device advertising only
 * the v1.1 INDIRECT_FIFO mechanism maps to std::errc::not_supported;
 * exhausted polling (blocking layer only) maps to std::errc::timed_out.
 * Results are returned by value only on success, so no output is ever
 * partially written.
 *
 * The single-transaction primitives never sleep; callers own all polling
 * delays. The blocking convenience layer (Target::writeImage/recover)
 * sleeps internally and must not be used inside an event loop.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace ocp::recovery
{

/* Recovery command codes (OCP Secure Firmware Recovery 1.0, table 8) */
enum class Cmd : uint8_t
{
    protCap = 0x22,
    deviceId = 0x23,
    deviceStatus = 0x24,
    deviceReset = 0x25,
    recoveryCtrl = 0x26,
    recoveryStatus = 0x27,
    hwStatus = 0x28,
    indirectCtrl = 0x29,
    indirectStatus = 0x2A,
    indirectData = 0x2B,
    vendor = 0x2C,
};

/*
 * Protocol code sets. Each set is a distinct scoped enum so the
 * overlapping numeric spaces cannot be mixed up (e.g. 0x03 is "recovery
 * mode" as a device status but "success" as a recovery status).
 * Reserved or vendor-defined wire values outside the enumerations are
 * passed through unchanged.
 */

/* DEVICE_STATUS status codes */
enum class DeviceStatus : uint8_t
{
    pending = 0x00,
    healthy = 0x01,
    deviceError = 0x02,
    recoveryMode = 0x03,
    recoveryPending = 0x04,
    runningRecovery = 0x05,
    bootFailure = 0x0E,
    fatalError = 0x0F,
};

/* DEVICE_STATUS protocol error codes */
enum class ProtocolError : uint8_t
{
    none = 0x00,
    unsupportedCommand = 0x01,
    unsupportedParameter = 0x02,
    lengthWriteError = 0x03,
    crcError = 0x04,
    notResponding = 0x05,
    general = 0xFF,
};

/* DEVICE_STATUS recovery reason codes (0x80+ are vendor unique) */
enum class ReasonCode : uint16_t
{
    noFailure = 0x00,
    hwError = 0x01,
    hwSoftError = 0x02,
    selfTestFailure = 0x03,
    criticalDataCorrupt = 0x04,
    keyManifestCorrupt = 0x05,
    keyManifestAuthFailure = 0x06,
    keyManifestAntiRollback = 0x07,
    bootLoaderCorrupt = 0x08,
    bootLoaderAuthFailure = 0x09,
    bootLoaderAntiRollback = 0x0A,
    mainImageCorrupt = 0x0B,
    mainImageAuthFailure = 0x0C,
    mainImageAntiRollback = 0x0D,
    recoveryImageCorrupt = 0x0E,
    recoveryImageAuthFailure = 0x0F,
    recoveryImageAntiRollback = 0x10,
    forcedRecovery = 0x11,
};

/* RECOVERY_STATUS status codes (low nibble) */
enum class RecoveryStatus : uint8_t
{
    notInRecovery = 0x00,
    awaitingImage = 0x01,
    bootingImage = 0x02,
    success = 0x03,
    failed = 0x0C,
    authError = 0x0D,
    enteredRecovery = 0x0E,
    invalidCms = 0x0F,
};

/* RECOVERY_CTRL recovery image selection */
enum class ImageSelection : uint8_t
{
    none = 0x00,
    fromCms = 0x01,
    stored = 0x02,
};

/* RECOVERY_CTRL activation */
enum class Activation : uint8_t
{
    none = 0x00,
    activate = 0x0F,
};

/* Largest INDIRECT_DATA payload per SMBus block write */
inline constexpr size_t chunkSizeMax = 252;

inline constexpr uint8_t cmsDefault = 0;

/* Poll budgets used by the blocking layer; exported so event-loop callers
 * can implement the same policy with their own sleep primitive. Devices
 * stall write acknowledgement while consuming a just-activated image;
 * successful writes acknowledge on the first poll, so a long interval
 * only costs time when the device is actually busy. */
inline constexpr unsigned int ackPollRetries = 5;
inline constexpr std::chrono::milliseconds ackPollInterval{1000};
inline constexpr unsigned int chunkWriteRetries = 3;
inline constexpr unsigned int statusPollRetries = 30;
inline constexpr std::chrono::milliseconds statusPollInterval{1000};

struct ProtCap
{
    static constexpr uint16_t capIndirectCtrl = 1U << 5;
    static constexpr uint16_t capIndirectFifo = 1U << 12;

    std::string magic; /* "OCP RECV" */
    uint8_t major;
    uint8_t minor;
    uint16_t caps;
    uint8_t numCms;
    uint8_t maxRespTime;     /* 2^n us */
    uint8_t heartbeatPeriod; /* 2^n us */

    /* True when the device positively advertises only the v1.1
     * INDIRECT_FIFO mechanism, which this library does not implement.
     * Devices commonly leave the capability bits unset, so absence of
     * capIndirectCtrl alone is not treated as unsupported. */
    [[nodiscard]] bool fifoOnly() const
    {
        return (caps & capIndirectFifo) != 0 && (caps & capIndirectCtrl) == 0;
    }
};

struct DeviceStatusInfo
{
    DeviceStatus status;
    ProtocolError protocolError;
    ReasonCode reason;
};

struct RecoveryStatusInfo
{
    RecoveryStatus status; /* low nibble */
    uint8_t imageIndex;    /* high nibble */
    uint8_t vendorStatus;
};

struct IndirectStatusInfo
{
    uint8_t status;     /* raw status byte (bit 2 = write acknowledge) */
    bool ack;           /* write-acknowledge bit, decoded */
    uint32_t sizeUnits; /* CMS size in 4-byte units; 0 when the device
                           returned a short response without the size
                           field (some implementations answer ack polls
                           with a status-only payload) */

    [[nodiscard]] uint64_t sizeBytes() const
    {
        return uint64_t{sizeUnits} * 4;
    }
};

/* One bus transaction: write wbuf; a non-empty rbuf requests a
 * repeated-start read into it. Returns a default-constructed (falsy)
 * error_code on success. Implementations must accept writes up to one
 * full protocol message (2 + chunkSizeMax bytes); the built-in
 * I2CTransport rejects anything larger. */
class Transport
{
  public:
    Transport() = default;
    virtual ~Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    Transport(Transport&&) = delete;
    Transport& operator=(Transport&&) = delete;

    [[nodiscard]] virtual std::error_code transfer(
        std::span<const uint8_t> wbuf, std::span<uint8_t> rbuf) = 0;
};

/* ioctl(I2C_RDWR) transport over /dev/i2c-<bus>; owns the fd. */
class I2CTransport final : public Transport
{
  public:
    [[nodiscard]] static std::expected<I2CTransport, std::error_code> open(
        uint16_t bus, uint16_t addr);

    ~I2CTransport() override;
    I2CTransport(I2CTransport&& other) noexcept;
    I2CTransport& operator=(I2CTransport&& other) noexcept;
    I2CTransport(const I2CTransport&) = delete;
    I2CTransport& operator=(const I2CTransport&) = delete;

    std::error_code transfer(std::span<const uint8_t> wbuf,
                             std::span<uint8_t> rbuf) override;

  private:
    I2CTransport(int fd, uint16_t addr) : fd(fd), addr(addr) {}

    int fd = -1;
    uint16_t addr = 0;
};

/* A recovery-capable device behind a Transport. */
class Target
{
  public:
    explicit Target(Transport& transport, size_t chunkSize = chunkSizeMax,
                    uint8_t cms = cmsDefault) :
        transport(transport), chunkSize(chunkSize), cms(cms)
    {}

    /* Single-transaction primitives (no sleeping) */

    [[nodiscard]] std::expected<ProtCap, std::error_code> getProtCap();

    /* Returns the device descriptor exactly as reported, truncated to
     * maxLen. The default matches the fixed 24-byte PCI/UUID descriptor
     * forms; devices with longer vendor-defined descriptors can be read
     * with maxLen up to one full SMBus block (255 bytes). maxLen also
     * sizes the bus read transaction, and some I2C adapters reject
     * reads longer than their quirk limit - keep the default unless the
     * device is known to report more. */
    [[nodiscard]] std::expected<std::vector<uint8_t>, std::error_code>
        getDeviceId(size_t maxLen = 24);

    [[nodiscard]] std::expected<DeviceStatusInfo, std::error_code>
        getDeviceStatus();

    [[nodiscard]] std::expected<RecoveryStatusInfo, std::error_code>
        getRecoveryStatus();

    /* DEVICE_RESET: reset the device into recovery mode with interface
     * mastering enabled ([0x01, 0x0F, 0x01]). */
    [[nodiscard]] std::expected<void, std::error_code> forceRecovery();

    [[nodiscard]] std::expected<void, std::error_code> recoveryCtrl(
        uint8_t window, ImageSelection imageSel, Activation activation);

    [[nodiscard]] std::expected<void, std::error_code> indirectCtrl(
        uint8_t window, uint32_t offset);

    [[nodiscard]] std::expected<IndirectStatusInfo, std::error_code>
        indirectStatus();

    [[nodiscard]] std::expected<void, std::error_code> writeIndirectData(
        std::span<const uint8_t> data);

    /* Block-read from the current indirect window (set with indirectCtrl),
     * e.g. to retrieve CMS log regions. Returns the number of bytes the
     * device produced (0 when drained). */
    [[nodiscard]] std::expected<size_t, std::error_code> readIndirectData(
        std::span<uint8_t> buf);

    /* Blocking convenience layer (sleeps with std::this_thread; for tests
     * and standalone tooling, NOT for use inside an event loop) */

    using ProgressFn = std::function<void(size_t written, size_t total)>;

    /* Stream image into the currently configured CMS window via chunked
     * INDIRECT_DATA writes with per-chunk acknowledge polling. */
    [[nodiscard]] std::expected<void, std::error_code> writeImage(
        std::span<const uint8_t> image, const ProgressFn& progress = {});

    /* Full recovery sequence: PROT_CAP (tolerant) -> force recovery mode
     * if needed -> stage image -> activate -> poll for success. */
    [[nodiscard]] std::expected<void, std::error_code> recover(
        std::span<const uint8_t> image, const ProgressFn& progress = {});

  private:
    [[nodiscard]] std::expected<void, std::error_code> command(
        Cmd cmd, std::span<const uint8_t> payload);

    /* Returns the SMBus block byte count of the payload in resp[1..];
     * counts below minCount are protocol errors. */
    [[nodiscard]] std::expected<size_t, std::error_code> commandRead(
        Cmd cmd, std::span<uint8_t> resp, size_t minCount = 1);

    [[nodiscard]] std::expected<void, std::error_code> waitWriteAck();

    Transport& transport;
    size_t chunkSize;
    uint8_t cms;
};

} // namespace ocp::recovery
