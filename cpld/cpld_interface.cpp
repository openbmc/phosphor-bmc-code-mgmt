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
    const std::string& chipName, uint16_t bus, uint8_t address) const
{
    auto it = creators.find(chipType);
    if (it != creators.end())
    {
        return (it->second)(ctx, chipName, bus, address);
    }
    return nullptr;
}

bool CPLDFactory::isSupported(const std::string& chipType) const
{
    return creators.contains(chipType);
}

} // namespace phosphor::software::cpld
