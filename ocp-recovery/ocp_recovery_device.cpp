#include "ocp_recovery_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <algorithm>
#include <chrono>

PHOSPHOR_LOG2_USING;

using namespace std::chrono_literals;

namespace phosphor::software::ocp_recovery::device
{

sdbusplus::async::task<bool> OCPRecoveryDevice::updateDevice(
    const uint8_t* image, size_t image_size)
{
    const std::vector<DeviceInf::ComponentImage> components = {
        {image, image_size, ""}};

    co_return co_await updateDeviceComponents(components);
}

sdbusplus::async::task<bool> OCPRecoveryDevice::updateDeviceComponents(
    const std::vector<DeviceInf::ComponentImage>& components)
{
    size_t componentBytes = 0;
    for (const auto& component : components)
    {
        componentBytes += component.size;
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
    struct ocp_ctx ocp;
    struct ocp_prot_cap cap;

    int rc = ocp_open(&ocp, bus, address);
    if (rc < 0)
    {
        error("Failed to open I2C bus {BUS}: {RC}", "BUS", bus, "RC", rc);
        co_return BusOutcome::unreachable;
    }

    ocp.chunk_size = chunkSize;
    ocp.cms = cms;

    rc = ocp_get_prot_cap(&ocp, &cap);
    if (rc == 0)
    {
        if (ocp_check_caps(&cap) < 0)
        {
            error(
                "Device on bus {BUS} only supports the INDIRECT_FIFO mechanism, which is not implemented",
                "BUS", bus);
            ocp_close(&ocp);
            co_return BusOutcome::failed;
        }
    }

    uint8_t status = 0;
    rc = ocp_get_device_status(&ocp, &status, NULL, NULL);
    if (rc < 0)
    {
        info("No OCP recovery device responding on bus {BUS} ({RC})", "BUS",
             bus, "RC", rc);
        ocp_close(&ocp);
        co_return BusOutcome::unreachable;
    }

    if (status == OCP_DEVICE_STATUS_HEALTHY)
    {
        info("Device on bus {BUS} is healthy; skipping recovery", "BUS", bus);
        ocp_close(&ocp);
        co_return BusOutcome::skippedHealthy;
    }

    if (!(co_await ensureRecoveryMode(&ocp, bus)))
    {
        ocp_close(&ocp);
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
            component.version, "SIZE", component.size, "CMS", componentCms);

        rc = ocp_recovery_ctrl(&ocp, componentCms, OCP_RECOVERY_IMAGE_FROM_CMS,
                               OCP_RECOVERY_NO_ACTIVATE);
        if (rc < 0)
        {
            error("bus {BUS}: RECOVERY_CTRL write failed: {RC}", "BUS", bus,
                  "RC", rc);
            ocp_close(&ocp);
            co_return BusOutcome::failed;
        }

        rc = ocp_indirect_ctrl(&ocp, componentCms, 0);
        if (rc < 0)
        {
            error("bus {BUS}: INDIRECT_CTRL write failed: {RC}", "BUS", bus,
                  "RC", rc);
            ocp_close(&ocp);
            co_return BusOutcome::failed;
        }

        if (!(co_await writeImage(&ocp, bus, component.image, component.size,
                                  written, grandTotal)))
        {
            ocp_close(&ocp);
            co_return BusOutcome::failed;
        }
    }

    // Activation is a one-shot action once every component image is
    // staged in its CMS region; activating between images kicks the
    // device out of its awaiting-image state and it rejects further
    // INDIRECT_DATA.
    rc = ocp_recovery_ctrl(&ocp, cms, OCP_RECOVERY_IMAGE_FROM_CMS,
                           OCP_RECOVERY_ACTIVATE);
    if (rc < 0)
    {
        error("bus {BUS}: recovery image activation failed: {RC}", "BUS", bus,
              "RC", rc);
        ocp_close(&ocp);
        co_return BusOutcome::failed;
    }

    const bool complete = co_await pollRecoveryComplete(&ocp, bus);

    ocp_close(&ocp);

    if (!complete)
    {
        co_return BusOutcome::failed;
    }

    info("bus {BUS}: OCP recovery of device {ADDR} succeeded", "BUS", bus,
         "ADDR", lg2::hex, address);

    co_return BusOutcome::recovered;
}

sdbusplus::async::task<bool> OCPRecoveryDevice::ensureRecoveryMode(
    struct ocp_ctx* ocp, uint16_t bus)
{
    uint8_t status = 0;
    uint8_t protocolError = 0;
    uint16_t reasonCode = 0;

    int rc = ocp_get_device_status(ocp, &status, &protocolError, &reasonCode);
    if (rc < 0)
    {
        error("bus {BUS}: DEVICE_STATUS read failed: {RC}", "BUS", bus, "RC",
              rc);
        co_return false;
    }

    if (status == OCP_DEVICE_STATUS_RECOVERY_MODE)
    {
        co_return true;
    }

    info(
        "bus {BUS}: device not in recovery mode (status {STATUS}, reason {REASON}), forcing recovery",
        "BUS", bus, "STATUS", lg2::hex, status, "REASON", lg2::hex,
        reasonCode);

    rc = ocp_force_recovery(ocp);
    if (rc < 0)
    {
        error("bus {BUS}: DEVICE_RESET write failed: {RC}", "BUS", bus, "RC",
              rc);
        co_return false;
    }

    for (uint32_t elapsed = 0; elapsed < forceRecoveryTimeout; elapsed++)
    {
        co_await sdbusplus::async::sleep_for(ctx, 1s);

        rc = ocp_get_device_status(ocp, &status, &protocolError, &reasonCode);
        if (rc == 0 && status == OCP_DEVICE_STATUS_RECOVERY_MODE)
        {
            co_return true;
        }
    }

    error(
        "bus {BUS}: device did not enter recovery mode within {TIMEOUT}s (status {STATUS})",
        "BUS", bus, "TIMEOUT", forceRecoveryTimeout, "STATUS", lg2::hex,
        status);

    co_return false;
}

sdbusplus::async::task<bool> OCPRecoveryDevice::writeImage(
    struct ocp_ctx* ocp, uint16_t bus, const uint8_t* image, size_t size,
    size_t& written, size_t grandTotal)
{
    size_t offset = 0;
    uint8_t lastProgress = 0;

    while (offset < size)
    {
        const size_t len = std::min(chunkSize, size - offset);
        bool acked = false;

        for (unsigned int attempt = 0;
             attempt < OCP_CHUNK_WRITE_RETRIES && !acked; attempt++)
        {
            int rc = ocp_indirect_data_write(ocp, &image[offset], len);
            if (rc < 0)
            {
                error(
                    "bus {BUS}: INDIRECT_DATA write at offset {OFFSET} failed: {RC}",
                    "BUS", bus, "OFFSET", offset, "RC", rc);
                co_return false;
            }

            for (unsigned int poll = 0; poll < OCP_ACK_POLL_RETRIES; poll++)
            {
                bool ack = false;

                rc = ocp_indirect_status(ocp, NULL, &ack, NULL);
                if (rc < 0)
                {
                    error("bus {BUS}: INDIRECT_STATUS read failed: {RC}",
                          "BUS", bus, "RC", rc);
                    co_return false;
                }
                if (ack)
                {
                    acked = true;
                    break;
                }

                co_await sdbusplus::async::sleep_for(
                    ctx, std::chrono::milliseconds(OCP_ACK_POLL_INTERVAL_MS));
            }
        }

        if (!acked)
        {
            error(
                "bus {BUS}: device did not acknowledge image data at offset {OFFSET}",
                "BUS", bus, "OFFSET", offset);
            co_return false;
        }

        offset += len;
        written += len;

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

sdbusplus::async::task<bool> OCPRecoveryDevice::pollRecoveryComplete(
    struct ocp_ctx* ocp, uint16_t bus)
{
    int rc;
    uint8_t status = OCP_RECOVERY_STATUS_NOT_IN_RECOVERY;

    for (unsigned int poll = 0; poll < OCP_STATUS_POLL_RETRIES; poll++)
    {
        rc = ocp_get_recovery_status(ocp, &status, NULL, NULL);
        if (rc == 0)
        {
            if (status == OCP_RECOVERY_STATUS_SUCCESS)
            {
                co_return true;
            }
            if (status >= OCP_RECOVERY_STATUS_FAILED)
            {
                error("bus {BUS}: recovery failed with status {STATUS}",
                      "BUS", bus, "STATUS", lg2::hex, status);
                co_return false;
            }
        }

        co_await sdbusplus::async::sleep_for(
            ctx, std::chrono::milliseconds(OCP_STATUS_POLL_INTERVAL_MS));
    }

    error("bus {BUS}: recovery did not complete in time (status {STATUS})",
          "BUS", bus, "STATUS", lg2::hex, status);

    co_return false;
}

} // namespace phosphor::software::ocp_recovery::device
