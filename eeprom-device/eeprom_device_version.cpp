#include "eeprom_device_version.hpp"

#include "kb9002/kb9002.hpp"
#include "pt5161l/pt5161l.hpp"

#include <functional>
#include <unordered_map>

using ProviderFactory = std::function<std::unique_ptr<DeviceVersion>(
    const std::string&, const uint16_t, const uint8_t)>;

template <typename ProviderType>
std::unique_ptr<DeviceVersion> createProvider(
    const std::string& chipModel, const uint16_t bus, const uint8_t address)
{
    return std::make_unique<ProviderType>(chipModel, bus, address);
}

static const std::unordered_map<std::string, ProviderFactory> providerMap = {
    {"PT5161LFirmware", createProvider<PT5161LDeviceVersion>},
    {"PT5081LFirmware", createProvider<PT5161LDeviceVersion>},
    {"KB9002Firmware", createProvider<KB9002DeviceVersion>}};

std::unique_ptr<DeviceVersion> getVersionProvider(
    const std::string& chipModel, const uint16_t bus, const uint8_t address)
{
    auto it = providerMap.find(chipModel);
    if (it != providerMap.end())
    {
        return it->second(chipModel, bus, address);
    }

    return nullptr;
}
