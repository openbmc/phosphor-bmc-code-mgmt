#include "vr.hpp"

#include <unordered_map>

namespace phosphor::software::VR
{

namespace
{
std::unordered_map<std::string, VRCreator>& registry()
{
    static std::unordered_map<std::string, VRCreator> reg;
    return reg;
}
} // namespace

void registerVR(std::string_view configType, VRCreator creator)
{
    registry().emplace(configType, std::move(creator));
}

std::unique_ptr<VoltageRegulator> create(sdbusplus::async::context& ctx,
                                         const std::string& configType,
                                         uint16_t bus, uint16_t address)
{
    auto it = registry().find(configType);
    if (it != registry().end())
    {
        return (it->second)(ctx, bus, address);
    }
    return nullptr;
}

bool isSupported(const std::string& configType)
{
    return registry().contains(configType);
}

} // namespace phosphor::software::VR
