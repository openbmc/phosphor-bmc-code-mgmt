// SPDX-License-Identifier: Apache-2.0

/*
 * ocp-recovery-tool - manual OCP Secure Firmware Recovery over I2C.
 *
 * A command-line front end for libocp, intended for bring-up and
 * scripted recovery of devices parked in their recovery ROM. The
 * regular update path is the phosphor-ocp-recovery-software-update
 * daemon; this tool talks the same protocol without any D-Bus
 * involvement.
 */

#include <ocp/ocp_recovery.h>

#include <CLI/CLI.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

// Result convention shared with existing OCP recovery tooling:
// 0 success, 1 recovery skipped (device operational), 2 failure.
enum RecoveryReturnCode : int
{
    success = 0,
    skipped = 1,
    failure = 2,
};

constexpr unsigned int forceRecoveryTimeoutSec = 30;

std::string deviceStatusStr(uint8_t status)
{
    switch (status)
    {
        case OCP_DEVICE_STATUS_PENDING:
            return "status pending";
        case OCP_DEVICE_STATUS_HEALTHY:
            return "device healthy";
        case OCP_DEVICE_STATUS_ERROR:
            return "device error";
        case OCP_DEVICE_STATUS_RECOVERY_MODE:
            return "recovery mode - ready to accept recovery image";
        case 0x04:
            return "recovery pending, waiting for device/platform reset";
        case 0x05:
            return "recovery image running";
        case OCP_DEVICE_STATUS_BOOT_FAILURE:
            return "boot failure";
        case OCP_DEVICE_STATUS_FATAL_ERROR:
            return "fatal error";
        default:
            return "reserved";
    }
}

std::string protocolErrorStr(uint8_t err)
{
    switch (err)
    {
        case 0x00:
            return "no protocol error";
        case 0x01:
            return "unsupported/read-only command";
        case 0x02:
            return "unsupported parameter";
        case 0x03:
            return "length write error";
        case 0x04:
            return "CRC error";
        case 0x05:
            return "device not responding";
        case 0xFF:
            return "general protocol error";
        default:
            return "reserved";
    }
}

std::string reasonCodeStr(uint16_t reason)
{
    switch (reason)
    {
        case 0x00:
            return "no boot failure detected";
        case 0x01:
            return "generic hardware error";
        case 0x02:
            return "generic hardware soft error";
        case 0x03:
            return "self-test failure";
        case 0x04:
            return "corrupted/missing critical data";
        case 0x05:
            return "missing/corrupt key manifest";
        case 0x06:
            return "authentication failure on key manifest";
        case 0x07:
            return "anti-rollback failure on key manifest";
        case 0x08:
            return "missing/corrupt boot loader firmware image";
        case 0x09:
            return "authentication failure on boot loader firmware image";
        case 0x0A:
            return "anti-rollback failure on boot loader firmware image";
        case 0x0B:
            return "missing/corrupt main/management firmware image";
        case 0x0C:
            return "authentication failure on main/management firmware image";
        case 0x0D:
            return "anti-rollback failure on main/management firmware image";
        case 0x0E:
            return "missing/corrupt recovery firmware";
        case 0x0F:
            return "authentication failure on recovery firmware";
        case 0x10:
            return "anti-rollback failure on recovery firmware";
        case 0x11:
            return "forced recovery";
        default:
            return (reason >= 0x80 && reason <= 0xFF) ? "vendor unique"
                                                      : "reserved";
    }
}

std::string recoveryStatusStr(uint8_t status)
{
    switch (status)
    {
        case OCP_RECOVERY_STATUS_NOT_IN_RECOVERY:
            return "not in recovery mode";
        case OCP_RECOVERY_STATUS_AWAITING_IMAGE:
            return "awaiting recovery image";
        case OCP_RECOVERY_STATUS_BOOTING_IMAGE:
            return "booting recovery image";
        case OCP_RECOVERY_STATUS_SUCCESS:
            return "recovery successful";
        case OCP_RECOVERY_STATUS_FAILED:
            return "recovery failed";
        case OCP_RECOVERY_STATUS_AUTH_ERROR:
            return "recovery image authentication error";
        case OCP_RECOVERY_STATUS_ENTERED_RECOVERY:
            return "error entering recovery mode";
        case OCP_RECOVERY_STATUS_INVALID_CMS:
            return "invalid component memory space";
        default:
            return "reserved";
    }
}

void progress(void* user, size_t written, size_t total)
{
    auto* last = static_cast<unsigned int*>(user);
    auto pct = static_cast<unsigned int>((written * 100) / total);

    if (pct / 10 != *last / 10 || written == total)
    {
        std::cout << std::format("  {}/{} bytes ({}%)\n", written, total, pct)
                  << std::flush;
        *last = pct;
    }
}

void sleepSec(unsigned int sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

bool readImage(const std::string& path, std::vector<uint8_t>& image)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
    {
        std::cerr << std::format("cannot open '{}'\n", path);
        return false;
    }

    auto size = f.tellg();
    if (size <= 0)
    {
        std::cerr << std::format("'{}' is empty\n", path);
        return false;
    }

    image.resize(static_cast<size_t>(size));
    f.seekg(0);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (!f.read(reinterpret_cast<char*>(image.data()), size))
    {
        std::cerr << std::format("cannot read '{}'\n", path);
        return false;
    }

    return true;
}

int cmdGetDeviceId(struct ocp_ctx* ctx)
{
    std::vector<uint8_t> id(32);
    size_t len = 0;

    int rc = ocp_get_device_id(ctx, id.data(), id.size(), &len);
    if (rc < 0)
    {
        std::cerr << std::format("DEVICE_ID read failed: {}\n",
                                 strerror(-rc));
        return failure;
    }

    std::cout << std::format("DEVICE_ID ({} bytes):", len);
    for (size_t i = 0; i < len; i++)
    {
        std::cout << std::format(" {:02x}", id[i]);
    }
    std::cout << "\n";
    return success;
}

int cmdGetDeviceStatus(struct ocp_ctx* ctx)
{
    struct ocp_prot_cap cap;
    uint8_t status = 0;
    uint8_t protErr = 0;
    uint16_t reason = 0;

    int rc = ocp_get_prot_cap(ctx, &cap);
    if (rc == 0)
    {
        std::cout << std::format(
            "PROT_CAP: magic='{}' version={}.{} caps={:#06x} cms={}\n",
            cap.magic, cap.major, cap.minor, cap.caps, cap.num_cms);
    }

    rc = ocp_get_device_status(ctx, &status, &protErr, &reason);
    if (rc < 0)
    {
        std::cerr << std::format("DEVICE_STATUS read failed: {}\n",
                                 strerror(-rc));
        return failure;
    }

    std::cout << std::format("Device status:   {:#04x} ({})\n", status,
                             deviceStatusStr(status));
    std::cout << std::format("Protocol error:  {:#04x} ({})\n", protErr,
                             protocolErrorStr(protErr));
    std::cout << std::format("Recovery reason: {:#06x} ({})\n", reason,
                             reasonCodeStr(reason));
    return success;
}

int cmdGetRecoveryStatus(struct ocp_ctx* ctx)
{
    uint8_t status = 0;
    uint8_t index = 0;
    uint8_t vendor = 0;

    int rc = ocp_get_recovery_status(ctx, &status, &index, &vendor);
    if (rc < 0)
    {
        std::cerr << std::format("RECOVERY_STATUS read failed: {}\n",
                                 strerror(-rc));
        return failure;
    }

    std::cout << std::format("Recovery status: {:#04x} ({})\n", status,
                             recoveryStatusStr(status));
    std::cout << std::format("Image index:     {}\n", index);
    std::cout << std::format("Vendor status:   {:#04x}\n", vendor);
    return success;
}

int cmdSetForceRecovery(struct ocp_ctx* ctx)
{
    int rc = ocp_force_recovery(ctx);
    if (rc < 0)
    {
        std::cerr << std::format("DEVICE_RESET write failed: {}\n",
                                 strerror(-rc));
        return failure;
    }
    std::cout << "forced recovery requested\n";
    return success;
}

int enterRecoveryMode(struct ocp_ctx* ctx)
{
    uint8_t status = 0;

    int rc = ocp_get_device_status(ctx, &status, nullptr, nullptr);
    if (rc < 0)
    {
        std::cerr << std::format("DEVICE_STATUS read failed: {}\n",
                                 strerror(-rc));
        return failure;
    }

    if (status == OCP_DEVICE_STATUS_RECOVERY_MODE)
    {
        return success;
    }

    if (status == OCP_DEVICE_STATUS_HEALTHY)
    {
        std::cout << "device is operational; skipping recovery\n";
        return skipped;
    }

    std::cout << std::format("device status {:#04x} ({}); forcing recovery\n",
                             status, deviceStatusStr(status));
    rc = ocp_force_recovery(ctx);
    if (rc < 0)
    {
        std::cerr << std::format("DEVICE_RESET write failed: {}\n",
                                 strerror(-rc));
        return failure;
    }

    for (unsigned int i = 0; i < forceRecoveryTimeoutSec; i++)
    {
        sleepSec(1);
        rc = ocp_get_device_status(ctx, &status, nullptr, nullptr);
        if (rc == 0 && status == OCP_DEVICE_STATUS_RECOVERY_MODE)
        {
            return success;
        }
    }

    std::cerr << std::format(
        "device did not enter recovery mode (status {:#04x})\n", status);
    return failure;
}

int cmdPerformRecovery(struct ocp_ctx* ctx,
                       const std::vector<std::string>& images)
{
    int rc = enterRecoveryMode(ctx);
    if (rc != success)
    {
        return rc;
    }

    size_t count = 0;
    for (const auto& path : images)
    {
        unsigned int lastPct = 100; // force first progress line
        std::vector<uint8_t> image;

        if (!readImage(path, image))
        {
            return failure;
        }

        count++;
        std::cout << std::format("[{}/{}] writing '{}' ({} bytes) to CMS {}\n",
                                 count, images.size(), path, image.size(),
                                 ctx->cms);

        rc = ocp_recovery_ctrl(ctx, ctx->cms, OCP_RECOVERY_IMAGE_FROM_CMS,
                               OCP_RECOVERY_NO_ACTIVATE);
        if (rc == 0)
        {
            rc = ocp_indirect_ctrl(ctx, ctx->cms, 0);
        }
        if (rc == 0)
        {
            rc = ocp_write_image(ctx, image.data(), image.size(), progress,
                                 &lastPct);
        }
        if (rc == 0)
        {
            rc = ocp_recovery_ctrl(ctx, ctx->cms, OCP_RECOVERY_IMAGE_FROM_CMS,
                                   OCP_RECOVERY_ACTIVATE);
        }

        if (rc < 0)
        {
            std::cerr << std::format("recovery of '{}' failed: {}\n", path,
                                     strerror(-rc));
            return failure;
        }
    }

    for (unsigned int poll = 0; poll < OCP_STATUS_POLL_RETRIES; poll++)
    {
        uint8_t status = 0;

        rc = ocp_get_recovery_status(ctx, &status, nullptr, nullptr);
        if (rc == 0)
        {
            if (status == OCP_RECOVERY_STATUS_SUCCESS)
            {
                std::cout << "recovery successful\n";
                return success;
            }
            if (status >= OCP_RECOVERY_STATUS_FAILED)
            {
                std::cerr << std::format("recovery failed: {:#04x} ({})\n",
                                         status, recoveryStatusStr(status));
                return failure;
            }
        }
        sleepSec(1);
    }

    std::cerr << "recovery did not complete in time\n";
    return failure;
}

int cmdGetCmsLogs(struct ocp_ctx* ctx, uint8_t window, std::string outfile)
{
    constexpr size_t readLimit = 1024 * 1024;
    std::vector<uint8_t> buf(255);
    uint32_t cmsSize = 0;
    size_t limit = readLimit;
    size_t total = 0;

    if (outfile.empty())
    {
        outfile = std::format("cms{}_log.bin", window);
    }

    int rc = ocp_indirect_ctrl(ctx, window, 0);
    if (rc < 0)
    {
        std::cerr << std::format("INDIRECT_CTRL write failed: {}\n",
                                 strerror(-rc));
        return failure;
    }

    rc = ocp_indirect_status(ctx, nullptr, nullptr, &cmsSize);
    if (rc == 0 && cmsSize > 0 && static_cast<size_t>(cmsSize) * 4 < limit)
    {
        limit = static_cast<size_t>(cmsSize) * 4; // size is in 4-byte units
    }

    std::ofstream f(outfile, std::ios::binary);
    if (!f)
    {
        std::cerr << std::format("cannot open '{}'\n", outfile);
        return failure;
    }

    while (total < limit)
    {
        size_t len = 0;

        rc = ocp_indirect_data_read(ctx, buf.data(), buf.size(), &len);
        if (rc < 0)
        {
            std::cerr << std::format("INDIRECT_DATA read failed: {}\n",
                                     strerror(-rc));
            return failure;
        }
        if (len == 0)
        {
            break;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!f.write(reinterpret_cast<const char*>(buf.data()),
                     static_cast<std::streamsize>(len)))
        {
            std::cerr << std::format("cannot write '{}'\n", outfile);
            return failure;
        }
        total += len;
    }

    std::cout << std::format("wrote {} bytes of CMS {} logs to '{}'\n", total,
                             window, outfile);
    return success;
}

} // namespace

int main(int argc, char** argv)
{
    CLI::App app{"OCP Secure Firmware Recovery tool"};

    int bus = -1;
    uint8_t address = 0;
    uint8_t cms = OCP_CMS_DEFAULT;
    size_t chunk = OCP_CHUNK_SIZE_MAX;
    uint8_t window = 2;
    std::string outfile;
    std::vector<std::string> images;

    app.add_option("-b,--bus", bus, "I2C bus number")->required();
    app.add_option("-s,--slave", address, "7-bit device address (e.g. 0x69)")
        ->required();
    app.add_option("-c,--cms", cms,
                   "Component memory space index (default 0)");
    app.add_option("-k,--chunk", chunk,
                   std::format("Block write payload size, 1-{} (default {})",
                               OCP_CHUNK_SIZE_MAX, OCP_CHUNK_SIZE_MAX))
        ->check(CLI::Range(size_t(1), size_t(OCP_CHUNK_SIZE_MAX)));

    auto* getDeviceId =
        app.add_subcommand("GetDeviceID", "Read and dump the DEVICE_ID record");
    auto* getDeviceStatus = app.add_subcommand(
        "GetDeviceStatus", "Decode PROT_CAP and DEVICE_STATUS");
    auto* getRecoveryStatus =
        app.add_subcommand("GetRecoveryStatus", "Decode RECOVERY_STATUS");
    auto* setForceRecovery = app.add_subcommand(
        "SetForceRecovery", "Reset the device into recovery mode");
    auto* performRecovery =
        app.add_subcommand("PerformOCPRecovery", "Perform OCP recovery");
    performRecovery
        ->add_option("-i,--image", images,
                     "Recovery image(s), staged and activated in order")
        ->required();
    auto* getCmsLogs =
        app.add_subcommand("GetCMSLogs", "Retrieve CMS logs to a file");
    getCmsLogs->add_option("-w,--window", window,
                           "Log window index (default 2)");
    getCmsLogs->add_option("-o,--outfile", outfile,
                           "Output file (default cms<w>_log.bin)");

    app.require_subcommand(1)->ignore_case();

    CLI11_PARSE(app, argc, argv);

    struct ocp_ctx ctx;
    int rc = ocp_open(&ctx, bus, address);
    if (rc < 0)
    {
        std::cerr << std::format("cannot open i2c bus {}: {}\n", bus,
                                 strerror(-rc));
        return failure;
    }
    ctx.cms = cms;
    ctx.chunk_size = chunk;

    int ret = failure;
    if (getDeviceId->parsed())
    {
        ret = cmdGetDeviceId(&ctx);
    }
    else if (getDeviceStatus->parsed())
    {
        ret = cmdGetDeviceStatus(&ctx);
    }
    else if (getRecoveryStatus->parsed())
    {
        ret = cmdGetRecoveryStatus(&ctx);
    }
    else if (setForceRecovery->parsed())
    {
        ret = cmdSetForceRecovery(&ctx);
    }
    else if (performRecovery->parsed())
    {
        ret = cmdPerformRecovery(&ctx, images);
    }
    else if (getCmsLogs->parsed())
    {
        ret = cmdGetCmsLogs(&ctx, window, outfile);
    }

    ocp_close(&ctx);
    return ret;
}
