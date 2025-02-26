#include "cpld_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "cpld.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::cpld;

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CPLDSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    std::optional<uint64_t> busNo = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Bus");
    std::optional<uint64_t> address =
        co_await dbusGetRequiredProperty<uint64_t>(ctx, service, path,
                                                   configIface, "Address");
    std::optional<std::string> chipType =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Type");
    std::optional<std::string> chipName =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "Name");

    if (!busNo.has_value() || !address.has_value() || !chipType.has_value() ||
        !chipName.has_value())
    {
        error("missing config property");
        co_return false;
    }

    lg2::debug(
        "CPLD device type: {TYPE} - {NAME} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", chipType.value(), "NAME", chipName.value(), "BUS",
        busNo.value(), "ADDR", address.value());

    auto cpld = std::make_unique<CPLDDevice>(
        ctx, chipType.value(), chipName.value(), busNo.value(), address.value(),
        config, this);

    std::string version = "unknown";

    std::unique_ptr<Software> software = std::make_unique<Software>(ctx, *cpld);

    software->setVersion(version);

    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    software->enableUpdate(allowedApplyTimes);

    cpld->softwareCurrent = std::move(software);

    devices.insert({config.objectPath, std::move(cpld)});

    co_return true;
}

void CPLDSoftwareManager::start()
{
    std::vector<std::string> configIntfs;
    auto configs = CPLDFactory::instance().getConfigs();
    configIntfs.reserve(configs.size());
    for (const auto& config : configs)
    {
        configIntfs.push_back("xyz.openbmc_project.Configuration." + config);
    }

    ctx.spawn(initDevices(configIntfs));
    ctx.run();
}

int main()
{
    sdbusplus::async::context ctx;

    CPLDSoftwareManager cpldSoftwareManager(ctx);

    cpldSoftwareManager.start();

    return 0;
}
