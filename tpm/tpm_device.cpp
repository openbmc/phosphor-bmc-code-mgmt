#include "tpm_device.hpp"

#include "tpm2/tpm2.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

TPMDevice::TPMDevice(sdbusplus::async::context& ctx, enum TPMType tpmType,
                     uint8_t tpmIndex, SoftwareConfig& config,
                     ManagerInf::SoftwareManager* parent) :
    Device(ctx, config, parent, {RequestedApplyTimes::OnReset})
{
    switch (tpmType)
    {
        case TPMType::TPM2:
            tpmInterface = std::make_unique<TPM2Interface>(ctx, tpmIndex);
            break;
        default:
            tpmInterface = nullptr;
            error("Unsupported TPM type: {TYPE}", "TYPE",
                  static_cast<int>(tpmType));
            break;
    }
}

sdbusplus::async::task<bool> TPMDevice::updateDevice(const uint8_t* image,
                                                     size_t imageSize)
{
    if (tpmInterface == nullptr)
    {
        error("TPM interface is not initialized");
        co_return false;
    }

    setUpdateProgress(10);

    if (!co_await tpmInterface->updateFirmware(image, imageSize))
    {
        error("Failed to update TPM firmware");
        co_return false;
    }

    setUpdateProgress(100);
    debug("Successfully updated TPM");
    co_return true;
}

sdbusplus::async::task<std::string> TPMDevice::getVersion()
{
    std::string version = "Unknown";

    if (tpmInterface == nullptr)
    {
        error("TPM interface is not initialized");
        co_return version;
    }

    if (!co_await tpmInterface->getVersion(version))
    {
        error("Failed to get TPM version");
    }

    co_return version;
}
