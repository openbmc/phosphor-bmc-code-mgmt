#include "ocp_recovery_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <algorithm>
#include <chrono>
#include <span>
#include <utility>

PHOSPHOR_LOG2_USING;

using namespace std::chrono_literals;

namespace phosphor::software::ocp_recovery::device
{

sdbusplus::async::task<bool> OCPRecoveryDevice::updateDevice(
    const uint8_t* image, size_t image_size)
{
    const std::vector<DeviceInf::ComponentImage> components = {
        {std::span<const uint8_t>(image, image_size), ""}};

    co_return co_await updateDeviceComponents(components);
}

sdbusplus::async::task<bool> OCPRecoveryDevice::updateDeviceComponents(
    const std::vector<DeviceInf::ComponentImage>& components)
{
    size_t componentBytes = 0;
    for (const auto& component : components)
    {
        componentBytes += component.image.size();
    }
    const size_t grandTotal = componentBytes * buses.size();

    size_t written = 0;
    size_t recovered = 0;
    size_t skipped = 0;
    size_t unreachable = 0;
    size_t failed = 0;

    setUpdateProgress(5);

    for (const uint16_t bus : buses)
    {
        const BusOutcome outcome =
            co_await recoverBus(bus, components, written, grandTotal);

        switch (outcome)
        {
            case BusOutcome::recovered:
                recovered++;
                break;
            case BusOutcome::skippedHealthy:
                skipped++;
                written += componentBytes;
                break;
            case BusOutcome::unreachable:
                unreachable++;
                written += componentBytes;
                break;
            case BusOutcome::failed:
                failed++;
                written += componentBytes;
                break;
        }
    }

    info(
        "OCP recovery sweep finished: {RECOVERED} recovered, {SKIPPED} healthy, {UNREACHABLE} unreachable, {FAILED} failed of {TOTAL} buses",
        "RECOVERED", recovered, "SKIPPED", skipped, "UNREACHABLE", unreachable,
        "FAILED", failed, "TOTAL", buses.size());

    if (failed != 0)
    {
        co_return false;
    }

    setUpdateProgress(100);

    co_return true;
}

sdbusplus::async::task<OCPRecoveryDevice::BusOutcome>
    OCPRecoveryDevice::recoverBus(
        uint16_t bus, const std::vector<DeviceInf::ComponentImage>& components,
        size_t& written, size_t grandTotal)
{
    auto transport = OCPRecov::I2CTransport::open(bus, address);
    if (!transport)
    {
        error("Failed to open I2C bus {BUS}: {ERROR}", "BUS", bus, "ERROR",
              transport.error().message());
        co_return BusOutcome::unreachable;
    }

    OCPRecov::Target target(*transport, chunkSize, cms);

    /* Tolerant: many devices do not implement PROT_CAP; only fail when
     * the device positively reports FIFO-only support. */
    if (const auto cap = target.getProtCap(); cap && cap->fifoOnly())
    {
        error(
            "Device on bus {BUS} only supports the INDIRECT_FIFO mechanism, which is not implemented",
            "BUS", bus);
        co_return BusOutcome::failed;
    }

    const auto status = target.getDeviceStatus();
    if (!status)
    {
        info("No OCP recovery device responding on bus {BUS} ({ERROR})", "BUS",
             bus, "ERROR", status.error().message());
        co_return BusOutcome::unreachable;
    }

    if (status->status == OCPRecov::DeviceStatus::healthy)
    {
        info("Device on bus {BUS} is healthy; skipping recovery", "BUS", bus);
        co_return BusOutcome::skippedHealthy;
    }

    if (!(co_await ensureRecoveryMode(target, bus)))
    {
        co_return BusOutcome::failed;
    }

    size_t index = 0;
    for (const auto& component : components)
    {
        // Successive component images are staged into successive CMS
        // regions: the device consumes the just-activated image and does
        // not accept further data into the same window.
        const auto componentCms = static_cast<uint8_t>(cms + index);
        index++;
        info(
            "bus {BUS}: [{INDEX}/{COUNT}] staging component '{VERSION}' ({SIZE} bytes) to CMS {CMS}",
            "BUS", bus, "INDEX", index, "COUNT", components.size(), "VERSION",
            component.version, "SIZE", component.image.size(), "CMS",
            componentCms);

        if (!(co_await stageComponent(target, bus, componentCms, component,
                                      written, grandTotal)))
        {
            co_return BusOutcome::failed;
        }
    }

    // Activation is a one-shot action once every component image is
    // staged in its CMS region; activating between images kicks the
    // device out of its awaiting-image state and it rejects further
    // INDIRECT_DATA.
    if (const auto activated =
            target.recoveryCtrl(cms, OCPRecov::ImageSelection::fromCms,
                                OCPRecov::Activation::activate);
        !activated)
    {
        error("bus {BUS}: recovery image activation failed: {ERROR}", "BUS",
              bus, "ERROR", activated.error().message());
        co_return BusOutcome::failed;
    }

    if (!(co_await pollRecoveryComplete(target, bus)))
    {
        co_return BusOutcome::failed;
    }

    info("bus {BUS}: OCP recovery of device {ADDR} succeeded", "BUS", bus,
         "ADDR", lg2::hex, address);

    co_return BusOutcome::recovered;
}

sdbusplus::async::task<bool> OCPRecoveryDevice::ensureRecoveryMode(
    OCPRecov::Target& target, uint16_t bus)
{
    auto status = target.getDeviceStatus();
    if (!status)
    {
        error("bus {BUS}: DEVICE_STATUS read failed: {ERROR}", "BUS", bus,
              "ERROR", status.error().message());
        co_return false;
    }

    if (status->status == OCPRecov::DeviceStatus::recoveryMode)
    {
        co_return true;
    }

    info(
        "bus {BUS}: device not in recovery mode (status {STATUS}, reason {REASON}), forcing recovery",
        "BUS", bus, "STATUS", lg2::hex, std::to_underlying(status->status),
        "REASON", lg2::hex, std::to_underlying(status->reason));

    if (const auto forced = target.forceRecovery(); !forced)
    {
        error("bus {BUS}: DEVICE_RESET write failed: {ERROR}", "BUS", bus,
              "ERROR", forced.error().message());
        co_return false;
    }

    for (uint32_t elapsed = 0; elapsed < forceRecoveryTimeout; elapsed++)
    {
        co_await sdbusplus::async::sleep_for(ctx, 1s);

        status = target.getDeviceStatus();
        if (status && status->status == OCPRecov::DeviceStatus::recoveryMode)
        {
            co_return true;
        }
    }

    error(
        "bus {BUS}: device did not enter recovery mode within {TIMEOUT}s (status {STATUS})",
        "BUS", bus, "TIMEOUT", forceRecoveryTimeout, "STATUS", lg2::hex,
        status ? std::to_underlying(status->status) : 0xFF);

    co_return false;
}

sdbusplus::async::task<bool> OCPRecoveryDevice::stageComponent(
    OCPRecov::Target& target, uint16_t bus, uint8_t window,
    const DeviceInf::ComponentImage& component, size_t& written,
    size_t grandTotal)
{
    if (const auto ctrl =
            target.recoveryCtrl(window, OCPRecov::ImageSelection::fromCms,
                                OCPRecov::Activation::none);
        !ctrl)
    {
        error("bus {BUS}: RECOVERY_CTRL write failed: {ERROR}", "BUS", bus,
              "ERROR", ctrl.error().message());
        co_return false;
    }

    if (const auto ctrl = target.indirectCtrl(window, 0); !ctrl)
    {
        error("bus {BUS}: INDIRECT_CTRL write failed: {ERROR}", "BUS", bus,
              "ERROR", ctrl.error().message());
        co_return false;
    }

    co_return co_await writeImage(target, bus, component.image, written,
                                  grandTotal);
}

sdbusplus::async::task<bool> OCPRecoveryDevice::writeImage(
    OCPRecov::Target& target, uint16_t bus, std::span<const uint8_t> image,
    size_t& written, size_t grandTotal)
{
    size_t offset = 0;
    uint8_t lastProgress = 0;

    while (offset < image.size())
    {
        const auto chunk =
            image.subspan(offset, std::min(chunkSize, image.size() - offset));

        if (!(co_await writeChunk(target, bus, chunk, offset)))
        {
            co_return false;
        }

        offset += chunk.size();
        written += chunk.size();

        const auto progress =
            static_cast<uint8_t>(10 + ((written * 80) / grandTotal));
        if (progress != lastProgress)
        {
            setUpdateProgress(progress);
            lastProgress = progress;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> OCPRecoveryDevice::writeChunk(
    OCPRecov::Target& target, uint16_t bus, std::span<const uint8_t> chunk,
    size_t offset)
{
    for (unsigned int attempt = 0; attempt < OCPRecov::chunkWriteRetries;
         attempt++)
    {
        if (const auto sent = target.writeIndirectData(chunk); !sent)
        {
            error(
                "bus {BUS}: INDIRECT_DATA write at offset {OFFSET} failed: {ERROR}",
                "BUS", bus, "OFFSET", offset, "ERROR", sent.error().message());
            co_return false;
        }

        for (unsigned int poll = 0; poll < OCPRecov::ackPollRetries; poll++)
        {
            const auto status = target.indirectStatus();
            if (!status)
            {
                error("bus {BUS}: INDIRECT_STATUS read failed: {ERROR}", "BUS",
                      bus, "ERROR", status.error().message());
                co_return false;
            }
            if (status->ack)
            {
                co_return true;
            }

            co_await sdbusplus::async::sleep_for(ctx,
                                                 OCPRecov::ackPollInterval);
        }
    }

    error("bus {BUS}: device did not acknowledge image data at offset {OFFSET}",
          "BUS", bus, "OFFSET", offset);
    co_return false;
}

sdbusplus::async::task<bool> OCPRecoveryDevice::pollRecoveryComplete(
    OCPRecov::Target& target, uint16_t bus)
{
    auto last = OCPRecov::RecoveryStatus::notInRecovery;

    for (unsigned int poll = 0; poll < OCPRecov::statusPollRetries; poll++)
    {
        if (const auto status = target.getRecoveryStatus())
        {
            last = status->status;
            if (last == OCPRecov::RecoveryStatus::success)
            {
                co_return true;
            }
            if (last >= OCPRecov::RecoveryStatus::failed)
            {
                error("bus {BUS}: recovery failed with status {STATUS}", "BUS",
                      bus, "STATUS", lg2::hex, std::to_underlying(last));
                co_return false;
            }
        }

        co_await sdbusplus::async::sleep_for(ctx, OCPRecov::statusPollInterval);
    }

    error("bus {BUS}: recovery did not complete in time (status {STATUS})",
          "BUS", bus, "STATUS", lg2::hex, std::to_underlying(last));

    co_return false;
}

} // namespace phosphor::software::ocp_recovery::device
