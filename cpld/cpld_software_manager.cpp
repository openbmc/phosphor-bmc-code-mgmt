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
    auto protocol = co_await dbusGetRequiredProperty<std::string>(
        ctx, service, path, configIface, "Protocol");
    auto jtagIndex = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "JtagIndex");

    if (!busNo.has_value() || !address.has_value() || !chipType.has_value() ||
        !chipName.has_value())
    {
        error("missing config property");
        co_return false;
    }

    // use I2C in default setting
    std::string protocolStr = "I2C";
    std::string jtagIndexStr = "";
    if (protocol.has_value())
    {
        if (protocol.value() == "JTAG")
        {
            protocolStr = protocol.value();
            if (!jtagIndex.has_value())
            {
                error("missing JtagIndex property on JTAG protocol");
                co_return false;
            }
            else
            {
                jtagIndexStr = std::to_string(jtagIndex.value());
            }
        }
    }

    lg2::info(
        "CPLD device type: {TYPE} - {NAME} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", chipType.value(), "NAME", chipName.value(), "BUS",
        busNo.value(), "ADDR", address.value());

    std::vector<std::string> gpioLines;
    std::vector<std::string> gpioValues;

    if (protocol.has_value())
    {
        if (protocol.value() == "JTAG")
        {
            const std::string configIfaceMux = configIface + ".MuxOutputs";
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

                gpioLines.push_back(name.value());
                gpioValues.push_back((polarity == "High") ? "1" : "0");
                info("MuxOutput{NUM} name: {NAME}, value: {VAL}", 
                     "NUM", std::to_string(i),
                     "NAME", gpioLines[i], "VAL", gpioValues[i]);
            }
        }
    }

    auto cpld = std::make_unique<CPLDDevice>(
        ctx, chipType.value(), chipName.value(), busNo.value(), address.value(),
        protocolStr, jtagIndexStr,
        gpioLines, gpioValues, config, this);

    std::string version = "unknown";
    if (!(co_await cpld->getVersion(version)))
    {
        lg2::error("Failed to get CPLD version for {NAME}", "NAME",
                   chipName.value());
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
