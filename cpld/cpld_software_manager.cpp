#include "cpld_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "cpld.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::cpld;

bool CPLDSoftwareManager::isSupported(const std::string& configType)
{
    return CPLDFactory::instance().isSupported(configType);
}

sdbusplus::async::task<bool> CPLDSoftwareManager::initDevice(
    const std::string& service, const sdbusplus::object_path& path,
    SoftwareConfig& config)
{
    auto busNo = config.getProperty<uint64_t>("Bus");
    auto address = config.getProperty<uint64_t>("Address");

    if (!busNo.has_value() || !address.has_value())
    {
        error("missing config property");
        co_return false;
    }

    lg2::debug(
        "CPLD device type: {TYPE} - {NAME} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", config.configType, "NAME", config.configName, "BUS",
        busNo.value(), "ADDR", address.value());

    const std::string configIfaceMux = config.baseInterface + ".MuxOutputs";

    std::vector<std::string> names;
    std::vector<bool> values;

    for (size_t i = 0; true; i++)
    {
        const std::string iface = configIfaceMux + std::to_string(i);

        auto name = config.getProperty<std::string>(iface, "Name");
        if (!name.has_value())
        {
            name = co_await dbusGetRequiredProperty<std::string>(
                ctx, service, path.str, iface, "Name");
        }

        auto polarity = config.getProperty<std::string>(iface, "Polarity");
        if (!polarity.has_value())
        {
            polarity = co_await dbusGetRequiredProperty<std::string>(
                ctx, service, path.str, iface, "Polarity");
        }

        if (!name.has_value() || !polarity.has_value())
        {
            break;
        }

        lg2::debug(
            "Found CPLD MuxOutput[{INDEX}]: Name={NAME}, Polarity={POLARITY}",
            "INDEX", i, "NAME", name.value(), "POLARITY", polarity.value());

        names.push_back(name.value());
        values.push_back((polarity == "High") ? 1 : 0);
    }

    lg2::debug("Total CPLD MuxOutputs found: {COUNT}", "COUNT", names.size());

    auto cpld = std::make_unique<CPLDDevice>(
        ctx, config.configType, config.configName, busNo.value(),
        address.value(), config, this, names, values);

    std::string version = "unknown";
    if (!(co_await cpld->getVersion(version)))
    {
        lg2::error("Failed to get CPLD version for {NAME}", "NAME",
                   config.configName);
    }

    std::unique_ptr<Software> software = std::make_unique<Software>(ctx, *cpld);

    software->setVersion(version, SoftwareVersion::VersionPurpose::Other);

    std::set<RequestedApplyTimes> allowedApplyTimes = {
        RequestedApplyTimes::Immediate, RequestedApplyTimes::OnReset};

    software->enableUpdate(allowedApplyTimes);

    cpld->softwareCurrent = std::move(software);

    devices.insert({config.objectPath, std::move(cpld)});

    co_return true;
}

void CPLDSoftwareManager::start()
{
    ctx.spawn(initDevices());
    ctx.run();
}

int main()
{
    sdbusplus::async::context ctx;

    CPLDSoftwareManager cpldSoftwareManager(ctx);

    cpldSoftwareManager.start();

    return 0;
}
