
#include "common/include/device_config.hpp"

#include <regex>
#include <stdexcept>

DeviceConfig::DeviceConfig(uint32_t vendorIANA, const std::string& compatible,
                           const std::string& configType,
                           const std::string& objPathConfig,
                           const std::string& name) :
    vendorIANA(vendorIANA), compatible(compatible), name(name),
    configType(configType), objPathConfig(objPathConfig)
{
    std::regex reCompatible("([a-zA-Z0-9])+(\\.([a-zA-Z0-9])+)+");
    std::cmatch m;

    if (name.empty())
    {
        throw std::invalid_argument(
            "invalid EM config 'Name' string: '" + name + "'");
    }

    // check compatible string with regex
    if (!std::regex_match(compatible.c_str(), m, reCompatible))
    {
        throw std::invalid_argument(
            "invalid compatible string: '" + compatible + "'");
    }

    if (!objPathConfig.contains("/"))
    {
        throw std::invalid_argument(
            "invalid object path: '" + compatible + "'");
    }
}
