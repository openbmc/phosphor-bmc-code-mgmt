#include "version_provider.hpp"

#include "pt5161l.hpp"

#include <functional>
#include <unordered_map>

namespace phosphor::software::eeprom::version
{

using ProviderFactory = std::function<std::unique_ptr<VersionProvider>(
    const std::string&, const uint8_t&, const uint8_t&)>;

template <typename ProviderType>
std::unique_ptr<VersionProvider> createProvider(
    const std::string& chipModel, const uint8_t& bus, const uint8_t& address)
{
    return std::make_unique<ProviderType>(chipModel, bus, address);
}

static const std::unordered_map<std::string, ProviderFactory> providerMap = {
    {"pt5161l", createProvider<PT5161LVersionProvider>}};

std::unique_ptr<VersionProvider> getVersionProvider(
    const std::string& chipModel, const uint8_t& bus, const uint8_t& address)
{
    auto it = providerMap.find(chipModel);
    if (it != providerMap.end())
    {
        return it->second(chipModel, bus, address);
    }

    return nullptr;
}

} // namespace phosphor::software::eeprom::version
