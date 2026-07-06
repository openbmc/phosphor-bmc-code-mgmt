#include "spi_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "spi_factory.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::manager
{

SPISoftwareManager::SPISoftwareManager(sdbusplus::async::context& ctx,
                                       bool isDryRun) :
    SoftwareManager(ctx, "SPIFlash"), dryRun(isDryRun)
{}

sdbusplus::async::task<bool> SPISoftwareManager::initDevice(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig& config)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    auto chipType = co_await dbusGetRequiredProperty<std::string>(
        ctx, service, path, configIface, "Type");
    auto chipName = co_await dbusGetRequiredProperty<std::string>(
        ctx, service, path, configIface, "Name");

    std::optional<uint64_t> spiControllerIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIControllerIndex");

    if (!spiControllerIndex.has_value())
    {
        error("Missing property: SPIControllerIndex");
        co_return false;
    }

    std::optional<uint64_t> spiDeviceIndex =
        co_await dbusGetRequiredProperty<uint64_t>(
            ctx, service, path, configIface, "SPIDeviceIndex");

    if (!spiDeviceIndex.has_value())
    {
        error("Missing property: SPIDeviceIndex");
        co_return false;
    }

    const std::string configIfaceMux = configIface + ".MuxOutputs";

    std::vector<std::string> names;
    std::vector<bool> values;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);

        std::optional<std::string> name =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Name");

        std::optional<std::string> polarity =
            co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                          iface, "Polarity");

        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }

        names.push_back(name.value());
        values.push_back((polarity == "High") ? 1 : 0);
    }

    debug("SPI device: {INDEX1}:{INDEX2}", "INDEX1", spiControllerIndex.value(),
          "INDEX2", spiDeviceIndex.value());

    auto spiDevice = SPIFactory::instance().create(
        chipType.value(), ctx, spiControllerIndex.value(),
        spiDeviceIndex.value(), dryRun, names, values, config, this);

    if (spiDevice == nullptr)
    {
        error("Unsupported SPI device type: {TYPE}", "TYPE", chipType.value());
        co_return false;
    }

    std::string version = spiDevice->getVersion();
    std::unique_ptr<Software> software =
        std::make_unique<Software>(ctx, *spiDevice);
    software->setVersion(version, SoftwareVersion::VersionPurpose::Host);

    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};
    software->enableUpdate(allowedApplyTimes);

    spiDevice->softwareCurrent = std::move(software);

    devices.emplace(config.objectPath, std::move(spiDevice));

    co_return true;
}

void SPISoftwareManager::start()
{
    std::vector<std::string> configIntfs;

    auto configs = SPIFactory::instance().getConfigs();
    configIntfs.reserve(configs.size());
    for (const auto& config : configs)
    {
        configIntfs.push_back("xyz.openbmc_project.Configuration." + config);
    }

    ctx.spawn(initDevices(configIntfs));
    ctx.run();
}

} // namespace phosphor::software::manager
