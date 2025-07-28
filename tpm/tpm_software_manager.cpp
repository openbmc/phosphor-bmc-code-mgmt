#include "tpm_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "tpm_device.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace SoftwareInf = phosphor::software;

void TPMSoftwareManager::start()
{
    std::vector<std::string> configIntfs = {
        "xyz.openbmc_project.Configuration.TPM2Firmware",
    };

    ctx.spawn(initDevices(configIntfs));
    ctx.run();
}

sdbusplus::async::task<bool> TPMSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
{
    const std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<std::string> type =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Type");
    TPMType tpmType;
    if (!stringToTPMType(type.value(), tpmType))
    {
        error("Invalid TPM type: {TYPE}", "TYPE", type.value());
        co_return false;
    }

    auto tpmDevice = std::make_unique<TPMDevice>(ctx, tpmType, config, this);

    std::unique_ptr<SoftwareInf::Software> software =
        std::make_unique<SoftwareInf::Software>(ctx, *tpmDevice);

    software->setVersion(co_await tpmDevice->getVersion());

    if (tpmDevice->isUpdateSupported())
    {
        std::set<RequestedApplyTimes> allowedApplyTimes = {
            RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

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
