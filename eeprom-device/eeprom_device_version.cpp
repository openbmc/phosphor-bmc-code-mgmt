#include "eeprom_device_version.hpp"

#include "common/include/dbus_helper.hpp"
#include "pt5161l/pt5161l.hpp"

#include <phosphor-logging/lg2.hpp>

#include <functional>
#include <unordered_map>

PHOSPHOR_LOG2_USING;

using ProviderFactory =
    std::function<sdbusplus::async::task<std::unique_ptr<DeviceVersion>>(
        sdbusplus::async::context&, const std::string&, const std::string&,
        const std::string&, const std::string&)>;

template <typename ProviderType>
sdbusplus::async::task<std::unique_ptr<DeviceVersion>> createProvider(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& configIface,
    const std::string& chipModel)
{
    std::optional<uint64_t> bus = co_await dbusGetRequiredProperty<uint64_t>(
        ctx, service, path, configIface, "Bus");

    std::optional<uint64_t> address =
        co_await dbusGetRequiredProperty<uint64_t>(ctx, service, path,
                                                   configIface, "Address");
    if (!bus.has_value() || !address.has_value())
    {
        error("{TYPE}: Missing I2C EEPROM device config property", "TYPE",
              chipModel);
        co_return nullptr;
    }

    co_return std::make_unique<ProviderType>(chipModel, bus.value(),
                                             address.value());
}

static const std::unordered_map<std::string, ProviderFactory> providerMap = {
    {"PT5161LFirmware", createProvider<PT5161LDeviceVersion>},
    {"PT5081LFirmware", createProvider<PT5161LDeviceVersion>}};

sdbusplus::async::task<std::unique_ptr<DeviceVersion>> getVersionProvider(
    sdbusplus::async::context& ctx, const std::string& service,
    const std::string& path, const std::string& configIface,
    const std::string& chipModel)
{
    auto it = providerMap.find(chipModel);
    if (it != providerMap.end())
    {
        co_return co_await it->second(ctx, service, path, configIface,
                                      chipModel);
    }

    co_return nullptr;
}
