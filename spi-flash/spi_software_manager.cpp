#include "spi_software_manager.hpp"

#include "common/include/dbus_helper.hpp"
#include "spi_factory.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <chrono>
#include <exception>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::manager
{

namespace
{

// A mux output property read can fail because Entity-Manager is slow to
// answer while it scans; that must not look like the end of the MuxOutputs
// list, or the device runs without its mux for the whole boot.
struct MuxProperty
{
    std::optional<std::string> value;
    bool absent = false; // the interface or property does not exist
    std::string error;   // the last failure, when there is no value
};

sdbusplus::async::task<MuxProperty> getMuxProperty(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& intf,
    const std::string& property)
{
    auto client =
        sdbusplus::async::proxy().service(service).path(path).interface(
            "org.freedesktop.DBus.Properties");

    MuxProperty result;
    try
    {
        auto value =
            co_await client.call<std::variant<std::string>>(ctx, "Get", intf,
                                                            property);
        result.value = std::get<std::string>(value);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        result.absent = isDbusAbsentError(e.name());
        result.error = e.what();
    }
    catch (const std::exception& e)
    {
        result.error = e.what();
    }
    co_return result;
}

} // namespace

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

    const std::string& chipType = config.configType;

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

        MuxProperty name;
        MuxProperty polarity;
        const auto start = std::chrono::steady_clock::now();
        for (int attempt = 1;; attempt++)
        {
            name = co_await getMuxProperty(ctx, service, path, iface, "Name");
            if (name.absent)
            {
                break;
            }
            polarity = co_await getMuxProperty(ctx, service, path, iface,
                                               "Polarity");
            if (name.value && polarity.value)
            {
                break;
            }
            if (std::chrono::steady_clock::now() - start >=
                dbusTransientRetryWindow)
            {
                break;
            }
            if (attempt == 1)
            {
                warning("Reading {INTF} failed, retrying: {ERROR}", "INTF",
                        iface, "ERROR",
                        name.value ? polarity.error : name.error);
            }
            co_await sdbusplus::async::sleep_for(ctx, dbusTransientRetryDelay);
        }

        if (name.absent)
        {
            // No MuxOutputs<i> interface: the end of the list.
            break;
        }

        if (!name.value || !polarity.value)
        {
            error("Incomplete mux configuration {INTF} on {PATH}: {ERROR}",
                  "INTF", iface, "PATH", path, "ERROR",
                  name.value ? polarity.error : name.error);
            co_return false;
        }

        names.push_back(name.value.value());
        values.push_back((polarity.value == "High") ? 1 : 0);
    }

    info("SPI flash {PATH}: {COUNT} mux output(s) configured", "PATH", path,
         "COUNT", names.size());

    debug("SPI device: {INDEX1}:{INDEX2}", "INDEX1", spiControllerIndex.value(),
          "INDEX2", spiDeviceIndex.value());

    auto spiDevice = SPIFactory::instance().create(
        chipType, ctx, spiControllerIndex.value(), spiDeviceIndex.value(),
        dryRun, names, values, config, this);

    if (spiDevice == nullptr)
    {
        error("Unsupported SPI device type: {TYPE}", "TYPE", chipType);
        co_return false;
    }

    std::string version = spiDevice->getVersion();
    std::unique_ptr<Software> software;
    try
    {
        software = std::make_unique<Software>(ctx, *spiDevice);
    }
    catch (const std::exception& e)
    {
        error("Failed to create software object for SPI device: {ERROR}",
              "ERROR", e);
        co_return false;
    }
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

    auto configs = SPIFactory::instance().getConfigInterfaceNames();
    configIntfs.reserve(configs.size());
    for (const auto& config : configs)
    {
        configIntfs.push_back("xyz.openbmc_project.Configuration." + config);
    }

    ctx.spawn(initDevices(configIntfs));
    ctx.run();
}

} // namespace phosphor::software::manager

int main(int argc, char* argv[])
{
    bool dryRun = false;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = std::string(argv[i]);
        if (arg == "--dryrun")
        {
            dryRun = true;
        }
    }

    sdbusplus::async::context ctx;

    phosphor::software::manager::SPISoftwareManager spiSoftwareManager(
        ctx, dryRun);

    spiSoftwareManager.start();

    return 0;
}
