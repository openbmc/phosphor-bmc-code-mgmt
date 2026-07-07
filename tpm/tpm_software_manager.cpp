#include "tpm_software_manager.hpp"

#include "tpm_device.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace SoftwareInf = phosphor::software;

void TPMSoftwareManager::start()
{
    ctx.spawn(initDevices());
    ctx.run();
}

bool TPMSoftwareManager::isSupported(const std::string& configType)
{
    TPMType tpmType;
    return stringToTPMType(configType, tpmType);
}

sdbusplus::async::task<bool> TPMSoftwareManager::initDevice(
    [[maybe_unused]] const std::string& service,
    [[maybe_unused]] const sdbusplus::object_path& path, SoftwareConfig& config)
{
    TPMType tpmType;
    stringToTPMType(config.configType, tpmType);

    auto tpmIndex = config.getProperty<uint64_t>("TPMIndex");
    if (!tpmIndex.has_value())
    {
        error("Missing property: TPMIndex");
        co_return false;
    }

    debug("TPM device: TPM Index={INDEX}, Type={TYPE}", "INDEX",
          tpmIndex.value(), "TYPE", config.configType);

    auto tpmDevice = std::make_unique<TPMDevice>(
        ctx, tpmType, static_cast<uint8_t>(tpmIndex.value()), config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *tpmDevice);

    software->setVersion(co_await tpmDevice->getVersion());

    if (tpmDevice->isUpdateSupported())
    {
        std::set<RequestedApplyTimes> allowedApplyTimes = {
            RequestedApplyTimes::OnReset};

        software->enableUpdate(allowedApplyTimes);
    }

    tpmDevice->softwareCurrent = std::move(software);

    devices.insert({config.objectPath, std::move(tpmDevice)});

    co_return true;
}

int main()
{
    sdbusplus::async::context ctx;

    TPMSoftwareManager tpmSoftwareManager(ctx);

    tpmSoftwareManager.start();
    return 0;
}
