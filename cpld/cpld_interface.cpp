#include "cpld_interface.hpp"

#include "lattice/interface.hpp"

namespace phosphor::software::cpld
{

CPLDFactory& CPLDFactory::instance()
{
    static CPLDFactory factory;
    return factory;
}

void CPLDFactory::registerCPLD(const std::string& vendorName, Creator creator)
{
    creators[vendorName] = std::move(creator);
}

std::unique_ptr<CPLDInterface> CPLDFactory::create(
    const std::string& vendorName, const std::string& chipName, uint16_t bus,
    uint8_t address) const
{
    auto it = creators.find(vendorName);
    if (it != creators.end())
    {
        return (it->second)(chipName, bus, address);
    }
    return nullptr;
}

std::vector<std::string> CPLDFactory::getConfigs()
{
    std::vector<std::string> configs;
    configs.reserve(creators.size());

    std::transform(creators.begin(), creators.end(),
                   std::back_inserter(configs),
                   [](const auto& pair) { return pair.first; });

    return configs;
}

} // namespace phosphor::software::cpld
