#pragma once

#include "gpio_controller.hpp"
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
        sdbusplus::async::context& ctx, const std::string& service,
        const sdbusplus::object_path& path, bool dryRun, SoftwareConfig& config,
        SoftwareManager* parent);

    static std::vector<std::string> getConfigInterfaceNames();
};

} // namespace phosphor::software::manager
