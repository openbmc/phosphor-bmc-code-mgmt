#include "common/include/software_config.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <regex>
#include <stdexcept>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software::config;

SoftwareConfig::SoftwareConfig(const std::string& objPath, uint32_t vendorIANA,
                               const std::string& compatible,
                               const std::string& configType,
                               const std::string& name) :
    objectPath(objPath), vendorIANA(vendorIANA), compatibleHardware(compatible),
    configName(name), configType(configType)
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
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::string> SoftwareConfig::getInventoryItemObjectPath(
    sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::vector<std::string> allInterfaces = {
        "xyz.openbmc_project.Inventory.Item.Board",
        "xyz.openbmc_project.Inventory.Item.Chassis",
    };

    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");
    auto res = co_await client.get_sub_tree(
        "/xyz/openbmc_project/inventory/system", 0, allInterfaces);

    for (auto& [path, v] : res)
    {
        debug("inventory item at path {PATH}", "PATH", path);

        // check if their path is a parent of our path
        if (objectPath.starts_with(path))
        {
            debug("found associated inventory item for {NAME}: {PATH}", "NAME",
                  configName, "PATH", path);
            co_return path;
        }
    }

    error("could not find associated inventory item for {NAME}", "NAME",
          configName);

    co_return "";
}
