#include "cpld_interface.hpp"

#include "lattice/interface.hpp"

namespace phosphor::software::cpld
{

CPLDFactory& CPLDFactory::instance()
{
    static CPLDFactory factory;
    return factory;
}

void CPLDFactory::registerVendor(const std::string& vendorName, Creator creator)
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

} // namespace phosphor::software::cpld
