#include "cpld_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "cpld.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::cpld;

sdbusplus::async::task<bool> CPLDSoftwareManager::initDevice(
    const std::string& service, const std::string& path, SoftwareConfig& config)
{
    std::string configIface =
        "xyz.openbmc_project.Configuration." + config.configType;

    auto busNo = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Bus");
    auto address = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Address");
    auto chipType = co_await dbusGetRequiredProperty<std::string>(
        ctx, service, path, configIface, "Type");
    auto chipName = co_await dbusGetRequiredProperty<std::string>(
        ctx, service, path, configIface, "Name");

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

    const std::string configIfaceMux = configIface + ".MuxOutputs";

    std::vector<std::string> names;
    std::vector<bool> values;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);
        lg2::info("Trying CPLD interface: {IFACE}", "IFACE", iface);

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

        lg2::info(
            "Found CPLD MuxOutput[{INDEX}]: Name={NAME}, Polarity={POLARITY}",
            "INDEX", i, "NAME", name.value(), "POLARITY", polarity.value());

        names.push_back(name.value());
        values.push_back((polarity == "High") ? 1 : 0);
    }

    lg2::info("Total CPLD MuxOutputs found: {COUNT}", "COUNT", names.size());

    auto cpld = std::make_unique<CPLDDevice>(
        ctx, chipType.value(), chipName.value(), busNo.value(), address.value(),
        config, this);
    cpld->setGpioConfig(names, values);

    std::string version = "unknown";
    if (!(co_await cpld->getVersion(version)))
    {
        lg2::error("Failed to get CPLD version for {NAME}", "NAME",
                   chipName.value());
    }

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
