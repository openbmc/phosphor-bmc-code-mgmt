#include "usb_rcm_driver.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <exception>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace phosphor::software::usb_rcm
{

namespace
{

// USB RCM protocol constants (NVIDIA Vera bootrom). The recovery interface and
// its bulk-OUT endpoint are per-device (DeviceConfig), not fixed constants.
constexpr uint8_t endpointNumberMask = 0x7F;   // bEndpointAddress number bits
constexpr size_t bulkChunkSize = 512;          // bulk transfer chunk
constexpr unsigned int bulkTimeoutMs = 30000;  // per-chunk bulk timeout
constexpr size_t maxPortDepth = 8;

// Vendor control transfer for 32-bit register reads.
constexpr uint8_t requestTypeRead = 0xC0; // device-to-host, vendor-specific
constexpr uint8_t requestCode = 0x00;
constexpr unsigned int controlTimeoutMs = 5000;
constexpr size_t registerSizeBytes = 4;

// Boot Progress-Code Queue register map.
constexpr uint32_t pcqEntryBase = 0x8000;   // queue entry array base
constexpr uint32_t pcqPointerReg = 0x2000;  // queue-0 pointer register
constexpr uint32_t pcqEntrySize = 8;        // timestamp(4) + progressCode(4)
constexpr uint32_t pcqCodeOffset = 4;
constexpr uint32_t pcqMaxEntries = 1024;
constexpr uint32_t pcqMaxIterations = 2048; // circular-walk safety bound
// Queue-0 pointer register (pcqPointerReg) bit fields.
constexpr uint32_t pcqEndIndexMask = 0x3FF; // bits [9:0]
constexpr uint32_t pcqStartIndexShift = 10; // bits [19:10]
constexpr uint32_t pcqStartIndexMask = 0x3FF;
constexpr uint32_t pcqQueueSizeShift = 20; // bits [30:20]
constexpr uint32_t pcqQueueSizeMask = 0x7FF;

// PLDM T5 ready (recovery complete) progress codes, per CPU.
constexpr uint32_t recoveryCompleteCpu0 = 0x70C2C002;
constexpr uint32_t recoveryCompleteCpu1 = 0x71C2C002;

// Progress-code field extraction.
constexpr uint32_t codeTypeProgress = 0x01;   // bits [31:30]
constexpr uint32_t codeTypeError = 0x02;      // bits [31:30]
constexpr uint8_t pscRomSubClass = 0xC0;      // bits [23:16]
constexpr uint8_t bootModeSelDoneOp = 0x02;   // PSC_ROM operation [5:0]
// PSC ROM mutable-DOT errors that are soft-ignorable with an empty DOT blob.
constexpr uint8_t mutableDotHeaderCheckFail = 0x0F; // operation bits [5:0]
constexpr uint8_t mutableDotIntegrityFail = 0x10;
constexpr uint8_t mutableDotSanityFail = 0x11;

// Progress-code poll cadence after each image transfer.
constexpr int progressPollIntervalMs = 500;
constexpr int postImageMonitorIterations = 10; // ~5s
constexpr int interImageDelayMs = 250;

// ECID (32 bytes) fields deciding blob requirement.
constexpr size_t ecidSize = 32;
constexpr size_t oemKeyValidByte = 28;
constexpr uint8_t oemKeyValidMask = 0x02;
constexpr size_t ownershipByteLow = 20;
constexpr size_t ownershipByteHigh = 21;
constexpr uint16_t ownershipMask = 0x1FF; // 9-bit fuse count

// Empty DOT blob: exactly 1024 zero bytes (PSC accepts and soft-continues).
constexpr size_t emptyDotBlobSize = 1024;

// USBRCMRecoveryErrorCode values (subset used by the driver).
constexpr int errDeviceNotFound = 0x01;
constexpr int errImageTransferFailed = 0x04;
constexpr int errFailedToReadData = 0x0D;
constexpr int errFileOpenFailure = 0x10;
constexpr int errUsbCommunicationError = 0x12;
constexpr int errDotBlobRequired = 0x13;
constexpr int errS2aBlobNotSupported = 0x14;

uint32_t codeType(uint32_t code)
{
    return (code >> 30) & 0x3;
}
uint8_t codeSubClass(uint32_t code)
{
    return static_cast<uint8_t>((code >> 16) & 0xFF);
}
uint8_t codeOperation(uint32_t code)
{
    return static_cast<uint8_t>(code & 0x3F);
}

bool isProgressCode(uint32_t code)
{
    return codeType(code) == codeTypeProgress;
}
// PSC_ROM BOOT_MODE_SEL_DONE marks that the bootrom has latched its boot
// selection; the boot mode lives in bit 0 of BOOT_SEL_VAL (code bits [12:6]).
bool isBootModeSelDone(uint32_t code)
{
    return isProgressCode(code) && codeSubClass(code) == pscRomSubClass &&
           codeOperation(code) == bootModeSelDoneOp;
}
bool bootModeIsRecovery(uint32_t code)
{
    return (((code >> 6) & 0x7F) & 0x01) == 0; // BootMode bit 0: 0 = Recovery
}
bool isRecoveryComplete(uint32_t code)
{
    return code == recoveryCompleteCpu0 || code == recoveryCompleteCpu1;
}

// One boot progress-code-queue entry (timestamp + progress code).
struct PcqEntry
{
    uint32_t timestamp;
    uint32_t code;
};

// Walk the CPU boot progress-code queue (a circular buffer in USB memory
// window 3) via the supplied 32-bit register reader, returning the valid
// entries in chronological order. Mirrors the downstream queue walk: the
// pointer register carries the start/end indices and the queue size, start ==
// end with a non-empty queue means full (not empty), and entries whose
// timestamp is invalid (0 or all-ones) are skipped. Returns empty on any read
// failure or an empty queue.
template <typename ReadReg>
std::vector<PcqEntry> readProgressQueue(ReadReg readReg)
{
    std::vector<PcqEntry> entries;
    std::optional<uint32_t> ptr = readReg(pcqPointerReg);
    if (!ptr.has_value())
    {
        return entries;
    }
    uint32_t endIndex = *ptr & pcqEndIndexMask;
    uint32_t startIndex = (*ptr >> pcqStartIndexShift) & pcqStartIndexMask;
    uint32_t queueSize = (*ptr >> pcqQueueSizeShift) & pcqQueueSizeMask;
    if (queueSize == 0 || startIndex >= queueSize || endIndex >= queueSize)
    {
        return entries;
    }
    // start == end with a non-empty queue means the queue is full.
    bool full = (startIndex == endIndex);
    uint32_t count =
        full ? queueSize : (endIndex + queueSize - startIndex) % queueSize;
    uint32_t idx = startIndex;
    for (uint32_t i = 0; i < count && i < pcqMaxIterations; ++i)
    {
        uint32_t tsAddr = pcqEntryBase + idx * pcqEntrySize;
        std::optional<uint32_t> ts = readReg(tsAddr);
        std::optional<uint32_t> code = readReg(tsAddr + pcqCodeOffset);
        if (!ts.has_value() || !code.has_value())
        {
            return {};
        }
        if (*ts != 0x00000000 && *ts != 0xFFFFFFFF)
        {
            entries.push_back({*ts, *code});
        }
        idx = (idx + 1) % queueSize;
    }
    std::sort(entries.begin(), entries.end(),
              [](const PcqEntry& a, const PcqEntry& b) {
                  return a.timestamp < b.timestamp;
              });
    return entries;
}

uint8_t swapNibbles(uint8_t value)
{
    return static_cast<uint8_t>(((value << 4) | (value >> 4)) & 0xFF);
}

std::string portPathOf(libusb_device* device)
{
    std::array<uint8_t, maxPortDepth> ports{};
    int len = libusb_get_port_numbers(device, ports.data(),
                                      static_cast<int>(ports.size()));
    if (len < 0)
    {
        return "";
    }
    std::string path = std::to_string(libusb_get_bus_number(device));
    for (int i = 0; i < len; ++i)
    {
        path += (i == 0 ? "-" : ".");
        path += std::to_string(ports[static_cast<size_t>(i)]);
    }
    return path;
}

// Resolves the RCM bulk-OUT endpoint on the recovery interface, returning its
// endpoint address (e.g. 0x08 or 0x0a) or nullopt if the interface is absent or
// exposes no matching bulk-OUT endpoint (i.e. the device is not in RCM mode).
// When the config pins an endpoint (recoveryEndpoint != 0) only that endpoint
// number is accepted; otherwise the interface's bulk-OUT endpoint is discovered
// from the descriptor, so silicon/board revisions that move it (0x08 vs 0x0a)
// work with no configuration at all.
std::optional<uint8_t> recoveryBulkOutEndpoint(libusb_device* device,
                                               int recoveryInterface,
                                               uint8_t recoveryEndpoint)
{
    libusb_config_descriptor* config = nullptr;
    if (libusb_get_active_config_descriptor(device, &config) != 0 ||
        config == nullptr)
    {
        return std::nullopt;
    }
    std::optional<uint8_t> endpoint;
    if (recoveryInterface < config->bNumInterfaces)
    {
        const libusb_interface& intf = config->interface[recoveryInterface];
        for (int a = 0; a < intf.num_altsetting && !endpoint; ++a)
        {
            const libusb_interface_descriptor& alt = intf.altsetting[a];
            for (uint8_t e = 0; e < alt.bNumEndpoints; ++e)
            {
                uint8_t addr = alt.endpoint[e].bEndpointAddress;
                bool isBulkOut =
                    (addr & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT &&
                    (alt.endpoint[e].bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) ==
                        LIBUSB_TRANSFER_TYPE_BULK;
                if (!isBulkOut)
                {
                    continue;
                }
                // Pinned endpoint: match by number. Otherwise take the first
                // bulk-OUT the interface advertises (auto-discovery).
                if (recoveryEndpoint != 0 &&
                    (addr & endpointNumberMask) != recoveryEndpoint)
                {
                    continue;
                }
                endpoint = addr;
                break;
            }
        }
    }
    libusb_free_config_descriptor(config);
    return endpoint;
}

// RAII: open a device, detach the kernel driver from and claim the recovery
// interface, restoring everything on destruction.
class ClaimedHandle
{
  public:
    ClaimedHandle(libusb_device* device, int iface) : recoveryInterface(iface)
    {
        if (libusb_open(device, &handle) != 0)
        {
            handle = nullptr;
            return;
        }
        if (libusb_kernel_driver_active(handle, recoveryInterface) == 1)
        {
            if (libusb_detach_kernel_driver(handle, recoveryInterface) == 0)
            {
                detached = true;
            }
        }
        // Tolerate EBUSY: the configuration may already be set.
        libusb_set_configuration(handle, 1);
        if (libusb_claim_interface(handle, recoveryInterface) != 0)
        {
            close();
            handle = nullptr;
            return;
        }
        claimed = true;
    }

    ~ClaimedHandle()
    {
        close();
    }

    ClaimedHandle(const ClaimedHandle&) = delete;
    ClaimedHandle& operator=(const ClaimedHandle&) = delete;
    ClaimedHandle(ClaimedHandle&&) = delete;
    ClaimedHandle& operator=(ClaimedHandle&&) = delete;

    libusb_device_handle* get() const
    {
        return handle;
    }

  private:
    void close()
    {
        if (handle == nullptr)
        {
            return;
        }
        if (claimed)
        {
            libusb_release_interface(handle, recoveryInterface);
        }
        if (detached)
        {
            libusb_attach_kernel_driver(handle, recoveryInterface);
        }
        libusb_close(handle);
    }

    libusb_device_handle* handle = nullptr;
    int recoveryInterface;
    bool detached = false;
    bool claimed = false;
};

struct BoardPrefixes
{
    std::string cpu;   // GPIO prefix for the CPU strap lines
    std::string reset; // GPIO prefix for the reset line
};

// The per-board GPIO name prefixes for each force-recovery layout.
std::vector<BoardPrefixes> boardPrefixes(ConfigType configType)
{
    switch (configType)
    {
        case ConfigType::c2:
            return {{"B0_M1_CPU", "B0_M1"}};
        case ConfigType::c1g2:
            return {{"BRD0_CPU", "BRD0"}};
        case ConfigType::c2g4:
            return {{"BRD0_CPU", "BRD0"}, {"BRD1_CPU", "BRD1"}};
    }
    return {};
}

// The strap (signal-suffix, value) pairs. RECOVERY_TYPE = 0b10 selects USB RCM;
// FORCED_RECOVERY_L is active-low (0 asserts recovery, 1 is the default state).
std::vector<std::pair<std::string, int>> strapValues(bool assertRecovery)
{
    return {
        {"FORCED_RECOVERY_L", assertRecovery ? 0 : 1},
        {"BOOT_DEV_SEL0", 0}, {"BOOT_DEV_SEL1", 0},
        {"BOOT_DEV_SEL2", 0}, {"RECOVERY_TYPE0", 0},
        {"RECOVERY_TYPE1", 1},
    };
}

} // namespace

std::optional<ConfigType> parseConfigType(const std::string& value)
{
    if (value == "c2")
    {
        return ConfigType::c2;
    }
    if (value == "c1g2")
    {
        return ConfigType::c1g2;
    }
    if (value == "c2g4")
    {
        return ConfigType::c2g4;
    }
    return std::nullopt;
}

const char* toString(RecoveryState state)
{
    switch (state)
    {
        case RecoveryState::inRecovery:
            return "in recovery";
        case RecoveryState::recoveryComplete:
            return "recovery complete";
        case RecoveryState::notInRecovery:
            return "not in recovery";
        case RecoveryState::unreachable:
            return "unreachable";
    }
    return "unreachable";
}

RecoveryDriver::RecoveryDriver(DeviceConfig config) : cfg(std::move(config)) {}

RecoveryDriver::~RecoveryDriver()
{
    if (usbCtx != nullptr)
    {
        libusb_exit(usbCtx);
    }
}

bool RecoveryDriver::ensureUsb()
{
    if (usbCtx != nullptr)
    {
        return true;
    }
    if (libusb_init(&usbCtx) != 0)
    {
        usbCtx = nullptr;
        lastErr = "failed to initialize libusb";
        return false;
    }
    return true;
}

libusb_device* RecoveryDriver::findDevice()
{
    if (!ensureUsb())
    {
        return nullptr;
    }

    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(usbCtx, &list);
    if (count < 0)
    {
        return nullptr;
    }

    libusb_device* match = nullptr;
    for (ssize_t i = 0; i < count; ++i)
    {
        libusb_device* dev = list[static_cast<size_t>(i)];
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(dev, &desc) != 0)
        {
            continue;
        }
        if (desc.idVendor != cfg.vendorId || desc.idProduct != cfg.productId)
        {
            continue;
        }
        if (portPathOf(dev) != cfg.usbPort)
        {
            continue;
        }
        match = libusb_ref_device(dev);
        break;
    }

    libusb_free_device_list(list, 1);
    return match;
}

std::optional<uint32_t> RecoveryDriver::readRegister32(
    libusb_device_handle* handle, uint32_t address)
{
    std::array<uint8_t, registerSizeBytes> buf{};
    int rc = libusb_control_transfer(
        handle, requestTypeRead, requestCode,
        static_cast<uint16_t>((address >> 16) & 0xFFFF),
        static_cast<uint16_t>(address & 0xFFFF), buf.data(),
        static_cast<uint16_t>(buf.size()), controlTimeoutMs);
    if (rc != static_cast<int>(registerSizeBytes))
    {
        return std::nullopt;
    }
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

std::optional<std::array<uint8_t, 32>> RecoveryDriver::readEcid(
    libusb_device* device, libusb_device_handle* handle)
{
    libusb_device_descriptor desc{};
    if (libusb_get_device_descriptor(device, &desc) != 0 ||
        desc.iSerialNumber == 0)
    {
        return std::nullopt;
    }

    std::array<uint8_t, 128> serial{};
    int len = libusb_get_string_descriptor_ascii(
        handle, desc.iSerialNumber, serial.data(),
        static_cast<int>(serial.size()));
    if (len < static_cast<int>(ecidSize * 2))
    {
        return std::nullopt;
    }

    std::array<uint8_t, ecidSize> ecid{};
    for (size_t i = 0; i < ecidSize; ++i)
    {
        const char* first = reinterpret_cast<const char*>(&serial[i * 2]);
        unsigned int byte = 0;
        auto res = std::from_chars(first, first + 2, byte, 16);
        if (res.ec != std::errc())
        {
            return std::nullopt;
        }
        ecid[i] = static_cast<uint8_t>(byte);
    }
    return ecid;
}

RecoveryDriver::BlobRequirement RecoveryDriver::blobRequirement(
    const std::array<uint8_t, 32>& ecid)
{
    // OEM-key-valid takes priority: those parts need an S2A blob (unsupported).
    if ((ecid[oemKeyValidByte] & oemKeyValidMask) != 0)
    {
        return BlobRequirement::s2a;
    }

    // Otherwise a DOT blob is required when the ownership fuse count is odd
    // (device is in a mutable-DOT state). Note: the exact parity rule should be
    // cross-checked against the downstream EcidParser for provisioned parts.
    uint16_t ownership = static_cast<uint16_t>(
        (static_cast<uint16_t>(swapNibbles(ecid[ownershipByteHigh])) << 8) |
        swapNibbles(ecid[ownershipByteLow]));
    ownership &= ownershipMask;
    if ((std::popcount(ownership) & 0x1) != 0)
    {
        return BlobRequirement::dot;
    }
    return BlobRequirement::none;
}

bool RecoveryDriver::sendBulkData(libusb_device_handle* handle,
                                  const uint8_t* data, size_t size,
                                  uint8_t recoveryEndpoint, int& libusbError)
{
    libusbError = 0;
    size_t sent = 0;
    while (sent < size)
    {
        int chunk = static_cast<int>(std::min(bulkChunkSize, size - sent));
        int actual = 0;
        int rc = libusb_bulk_transfer(handle, recoveryEndpoint,
                                      const_cast<uint8_t*>(data + sent), chunk,
                                      &actual, bulkTimeoutMs);
        if (rc != 0)
        {
            libusbError = rc;
            return false;
        }
        if (actual <= 0)
        {
            return false;
        }
        sent += static_cast<size_t>(actual);
    }
    return true;
}

RecoveryDriver::ProgressOutcome RecoveryDriver::monitorProgress(
    libusb_device_handle* handle)
{
    ProgressOutcome outcome;
    for (int iteration = 0; iteration < postImageMonitorIterations; ++iteration)
    {
        std::optional<uint32_t> ptr = readRegister32(handle, pcqPointerReg);
        if (ptr.has_value())
        {
            uint32_t endIndex = *ptr & 0x3FF;
            for (uint32_t idx = 0; idx < endIndex && idx < pcqMaxEntries; ++idx)
            {
                uint32_t entryAddr = pcqEntryBase + idx * pcqEntrySize;
                std::optional<uint32_t> ts = readRegister32(handle, entryAddr);
                std::optional<uint32_t> code =
                    readRegister32(handle, entryAddr + pcqCodeOffset);
                if (!ts.has_value() || !code.has_value() || *ts == 0 ||
                    *ts == 0xFFFFFFFF)
                {
                    continue;
                }
                outcome.lastCode = *code;

                if (*code == recoveryCompleteCpu0 ||
                    *code == recoveryCompleteCpu1)
                {
                    outcome.complete = true;
                    return outcome;
                }
                if (codeType(*code) == codeTypeError)
                {
                    uint8_t op = codeOperation(*code);
                    bool mutableDot = codeSubClass(*code) == pscRomSubClass &&
                                      (op == mutableDotHeaderCheckFail ||
                                       op == mutableDotIntegrityFail ||
                                       op == mutableDotSanityFail);
                    if (mutableDot)
                    {
                        outcome.mutableDotSoftError = true;
                    }
                    else
                    {
                        outcome.fatalError = true;
                        return outcome;
                    }
                }
            }
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(progressPollIntervalMs));
    }
    return outcome;
}

std::string RecoveryDriver::dotBlobPath() const
{
    // Derive the per-CPU blob from the trailing index of the device name:
    // "..._0" -> <dotBlobDir>/CPU_0.bin.
    size_t pos = cfg.name.find_last_not_of("0123456789");
    if (pos == std::string::npos || pos + 1 >= cfg.name.size())
    {
        return "";
    }
    std::string suffix = cfg.name.substr(pos + 1);
    return cfg.dotBlobDir + "/CPU_" + suffix + ".bin";
}

bool RecoveryDriver::driveStraps(bool assertRecovery)
{
    std::vector<BoardPrefixes> boards = boardPrefixes(cfg.configType);
    if (boards.empty())
    {
        lastErr = "unknown USB RCM config type";
        return false;
    }

    try
    {
        // Keep every requested line alive (and thus driven) until scope exit.
        std::vector<gpiod::line> lines;
        auto request = [&lines](const std::string& name, int value) -> bool {
            gpiod::line line = gpiod::find_line(name);
            if (!line)
            {
                return false;
            }
            line.request(
                {"usb_rcm_recovery", gpiod::line_request::DIRECTION_OUTPUT, 0},
                value);
            lines.push_back(std::move(line));
            return true;
        };

        if (assertRecovery)
        {
            // Phase 1: assert reset (active-low) on every board, held through
            // the strap-write window (on some HW one board's reset resets the
            // other, so all are held together).
            std::vector<size_t> resetIndex;
            for (const BoardPrefixes& board : boards)
            {
                const std::string name = board.reset + "_IST_SYS_RST_L-O";
                if (!request(name, 0))
                {
                    lastErr = "GPIO line not found: " + name;
                    return false;
                }
                resetIndex.push_back(lines.size() - 1);
            }
            // Phase 2: program the recovery straps on every board.
            for (const BoardPrefixes& board : boards)
            {
                for (const auto& [signal, value] : strapValues(true))
                {
                    const std::string name = board.cpu + "_" + signal + "-O";
                    if (!request(name, value))
                    {
                        lastErr = "GPIO line not found: " + name;
                        return false;
                    }
                }
            }
            // Phase 3: release reset on every board.
            for (size_t i : resetIndex)
            {
                lines[i].set_value(1);
            }
        }
        else
        {
            // Restore default cold-boot strap states (no reset toggle).
            for (const BoardPrefixes& board : boards)
            {
                for (const auto& [signal, value] : strapValues(false))
                {
                    const std::string name = board.cpu + "_" + signal + "-O";
                    if (!request(name, value))
                    {
                        lastErr = "GPIO line not found: " + name;
                        return false;
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lastErr = std::string("GPIO error: ") + e.what();
        return false;
    }
    return true;
}

bool RecoveryDriver::forceRecovery()
{
    return driveStraps(true);
}

bool RecoveryDriver::clearForceRecovery()
{
    return driveStraps(false);
}

RecoveryState RecoveryDriver::status()
{
    if (!ensureUsb())
    {
        return RecoveryState::unreachable;
    }

    libusb_device** list = nullptr;
    ssize_t count = libusb_get_device_list(usbCtx, &list);
    if (count < 0)
    {
        return RecoveryState::unreachable;
    }

    RecoveryState state = RecoveryState::unreachable;
    libusb_device* rcmDevice = nullptr;
    for (ssize_t i = 0; i < count; ++i)
    {
        libusb_device* dev = list[static_cast<size_t>(i)];
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(dev, &desc) != 0)
        {
            continue;
        }
        if (desc.idVendor != cfg.vendorId || portPathOf(dev) != cfg.usbPort)
        {
            continue;
        }
        if (desc.idProduct == cfg.productId &&
            recoveryBulkOutEndpoint(dev, cfg.recoveryInterface,
                                    cfg.recoveryEndpoint)
                .has_value())
        {
            state = RecoveryState::inRecovery;
            rcmDevice = libusb_ref_device(dev);
        }
        else
        {
            state = RecoveryState::notInRecovery;
        }
        break;
    }
    libusb_free_device_list(list, 1);

    // If in RCM mode, a completed recovery reports PLDM T5 ready in the queue.
    if (rcmDevice != nullptr)
    {
        ClaimedHandle handle(rcmDevice, cfg.recoveryInterface);
        if (handle.get() != nullptr)
        {
            // Refine the state from the boot progress-code queue, mirroring the
            // downstream reference: find the newest BOOT_MODE_SEL_DONE to gate
            // on the actual boot mode, then look only at the latest progress
            // code from that marker onward - rather than matching a completion
            // code anywhere in the queue (which may be stale from a prior
            // cycle).
            libusb_device_handle* h = handle.get();
            std::vector<PcqEntry> entries = readProgressQueue(
                [h](uint32_t addr) { return readRegister32(h, addr); });

            std::optional<size_t> bootIdx;
            for (size_t i = entries.size(); i-- > 0;)
            {
                if (isBootModeSelDone(entries[i].code))
                {
                    bootIdx = i;
                    break;
                }
            }

            if (bootIdx.has_value())
            {
                if (!bootModeIsRecovery(entries[*bootIdx].code))
                {
                    // Enumerated in RCM but the bootrom selected a normal boot.
                    state = RecoveryState::notInRecovery;
                }
                else
                {
                    // Newest progress code from the boot-mode marker onward.
                    std::optional<uint32_t> latest;
                    for (size_t i = entries.size(); i-- > *bootIdx;)
                    {
                        if (isProgressCode(entries[i].code))
                        {
                            latest = entries[i].code;
                            break;
                        }
                    }
                    state = (latest.has_value() && isRecoveryComplete(*latest))
                                ? RecoveryState::recoveryComplete
                                : RecoveryState::inRecovery;
                }
            }
            // No BOOT_MODE_SEL_DONE (or an unreadable queue) keeps the
            // RCM-enumeration verdict of inRecovery set above.
        }
        libusb_unref_device(rcmDevice);
    }

    return state;
}

RecoveryResult RecoveryDriver::recover(const std::vector<Image>& images,
                                       bool verbose)
{
    (void)verbose;
    RecoveryResult result;

    if (cfg.usbPort.empty())
    {
        result.error = "no USB port configured";
        result.errorCode = errDeviceNotFound;
        return result;
    }
    if (images.empty())
    {
        result.error = "no images to send";
        result.errorCode = errImageTransferFailed;
        return result;
    }

    libusb_device* device = findDevice();
    if (device == nullptr)
    {
        result.error = "RCM device not found on port " + cfg.usbPort;
        result.errorCode = errDeviceNotFound;
        return result;
    }

    ClaimedHandle handle(device, cfg.recoveryInterface);
    if (handle.get() == nullptr)
    {
        libusb_unref_device(device);
        result.error = "failed to open/claim RCM device";
        result.errorCode = errUsbCommunicationError;
        return result;
    }

    // Resolve the bulk-OUT endpoint to stream to: the config-pinned endpoint if
    // set, otherwise the recovery interface's bulk-OUT discovered from the
    // descriptor. Absence means the device is not actually in RCM mode.
    std::optional<uint8_t> bulkEndpoint = recoveryBulkOutEndpoint(
        device, cfg.recoveryInterface, cfg.recoveryEndpoint);
    if (!bulkEndpoint)
    {
        libusb_unref_device(device);
        result.error = "RCM device exposes no bulk-OUT endpoint on interface " +
                       std::to_string(cfg.recoveryInterface);
        result.errorCode = errUsbCommunicationError;
        return result;
    }

    // ECID gates the DOT blob. If the ECID cannot be read, proceed without a
    // blob (matches downstream behaviour).
    std::string blobPath;
    bool emptyDotBlob = false;
    if (auto ecid = readEcid(device, handle.get()))
    {
        BlobRequirement req = blobRequirement(*ecid);
        if (req == BlobRequirement::s2a)
        {
            libusb_unref_device(device);
            result.error = "S2A blob required - not supported";
            result.errorCode = errS2aBlobNotSupported;
            return result;
        }
        if (req == BlobRequirement::dot)
        {
            blobPath = dotBlobPath();
            if (blobPath.empty())
            {
                libusb_unref_device(device);
                result.error = "DOT blob required but none available";
                result.errorCode = errDotBlobRequired;
                return result;
            }
        }
    }

    // Build the ordered send list: DOT blob (from disk) first, then the images.
    std::vector<uint8_t> blobData;
    std::vector<Image> sendList;
    if (!blobPath.empty())
    {
        std::ifstream file(blobPath, std::ios::binary);
        if (!file.is_open())
        {
            libusb_unref_device(device);
            result.error = "cannot open DOT blob " + blobPath;
            result.errorCode = errFileOpenFailure;
            return result;
        }
        blobData.assign(std::istreambuf_iterator<char>(file),
                        std::istreambuf_iterator<char>());
        emptyDotBlob =
            blobData.size() == emptyDotBlobSize &&
            std::all_of(blobData.begin(), blobData.end(),
                        [](uint8_t b) { return b == 0; });
        sendList.push_back({blobData.data(), blobData.size(), "DOT blob"});
    }
    sendList.insert(sendList.end(), images.begin(), images.end());

    // Stream each blob; poll the progress queue after each.
    for (const Image& image : sendList)
    {
        int libusbError = 0;
        if (!sendBulkData(handle.get(), image.data, image.size, *bulkEndpoint,
                          libusbError))
        {
            // After an accepted empty DOT blob the first image may time out as
            // the PSC ROM re-enters recovery: treat that as soft success.
            if (emptyDotBlob && libusbError == LIBUSB_ERROR_TIMEOUT)
            {
                libusb_unref_device(device);
                result.success = true;
                result.emptyDotAccepted = true;
                return result;
            }
            libusb_unref_device(device);
            result.error = "bulk transfer of '" + image.label + "' failed";
            result.errorCode = (libusbError == LIBUSB_ERROR_TIMEOUT)
                                   ? errImageTransferFailed
                                   : errFailedToReadData;
            return result;
        }

        ProgressOutcome outcome = monitorProgress(handle.get());
        if (outcome.complete)
        {
            libusb_unref_device(device);
            result.success = true;
            return result;
        }
        if (outcome.fatalError)
        {
            libusb_unref_device(device);
            result.error = "device reported error progress code";
            result.errorCode = errImageTransferFailed;
            return result;
        }
        if (emptyDotBlob && outcome.mutableDotSoftError &&
            image.label == "DOT blob")
        {
            result.emptyDotAccepted = true;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interImageDelayMs));
    }

    libusb_unref_device(device);

    // No fatal error after all transfers: success (progress-code completion may
    // not have been observed within the monitor window).
    result.success = true;
    return result;
}

} // namespace phosphor::software::usb_rcm
