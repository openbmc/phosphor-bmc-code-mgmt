#include "cpld_interface.hpp"

namespace phosphor::software::cpld
{

CPLDFactory& CPLDFactory::instance()
{
    static CPLDFactory factory;
    return factory;
}

void CPLDFactory::registerCPLD(const std::string& chipType, Creator creator)
{
    creators[chipType] = std::move(creator);
}

std::unique_ptr<CPLDInterface> CPLDFactory::create(
    const std::string& chipType, sdbusplus::async::context& ctx,
    const std::string& chipName, const std::string& protocol,
    const std::string& jtagIndex, uint16_t bus, uint8_t address,
    const std::vector<std::string>& gpioLines,
    const std::vector<std::string>& gpioValues) const
{
    auto it = creators.find(chipType);
    if (it != creators.end())
    {
        return (it->second)(ctx, chipName, protocol, jtagIndex,
                            bus, address, gpioLines, gpioValues);
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
