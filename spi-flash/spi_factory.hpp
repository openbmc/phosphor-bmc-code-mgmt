#pragma once

#include "spi_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::manager
{

class SPIFactory
{
  public:
    static SPIFactory& instance();

    static sdbusplus::async::task<std::unique_ptr<SPIDevice>> create(
        const std::string& chipType, sdbusplus::async::context& ctx,
        uint64_t spiControllerIndex, uint64_t spiDeviceIndex, bool dryRun,
        const std::vector<std::string>& names, const std::vector<bool>& values,
        SoftwareConfig& config, SoftwareManager* parent,
        const std::string& service, const sdbusplus::object_path& path,
        const std::string& configIface);

    static std::vector<std::string> getConfigInterfaceNames();
};

} // namespace phosphor::software::manager
