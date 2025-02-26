#include "cpld_code_updater.hpp"

#include "cpld.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::cpld;
using namespace phosphor::software::cpld::device;

template <typename T>
sdbusplus::async::task<std::optional<T>> dbusGetRequiredProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    auto client =
        sdbusplus::async::proxy().service(service).path(path).interface(
            "org.freedesktop.DBus.Properties");

    try
    {
        std::variant<T> result =
            co_await client.call<std::variant<T>>(ctx, "Get", intf, property);

        co_return std::get<T>(result);
    }
    catch (const std::exception& e)
    {
        error("Missing property {PROPERTY} on path {PATH}, interface {INTF}",
              "PROPERTY", property, "PATH", path, "INTF", intf);
    }

    co_return std::nullopt;
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<bool> CPLDCodeUpdater::initDevice(
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
    std::optional<std::string> chipVendor =
        co_await dbusGetRequiredProperty<std::string>(
            ctx, service, path, configIface, "ChipVendor");
    std::optional<std::string> chipName =
        co_await dbusGetRequiredProperty<std::string>(ctx, service, path,
                                                      configIface, "ChipName");

    if (!busNo.has_value() || !address.has_value() || !chipVendor.has_value() ||
        !chipName.has_value())
    {
        error("missing config property");
        co_return false;
    }

    // Check correctness
    lg2::debug(
        "[config] CPLD type: {TYPE} - {NAME} on Bus: {BUS} at Address: {ADDR}",
        "TYPE", chipVendor.value(), "NAME", chipName.value(), "BUS",
        busNo.value(), "ADDR", address.value());

    auto cpld = std::make_unique<phosphor::software::cpld::device::CPLDDevice>(
        ctx, chipVendor.value(), chipName.value(), busNo.value(),
        address.value(), config, this);

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
