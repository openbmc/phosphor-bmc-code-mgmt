#pragma once

#include "common/include/host_power.hpp"

#include <xyz/openbmc_project/State/Host/client.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

using phosphor::software::host_power::HostState;

namespace phosphor::software::eeprom
{

class DeviceVersion
{
  public:
    DeviceVersion(const std::string& chipModel, const uint8_t& bus,
                  const uint8_t& address) :
        chipModel(chipModel), bus(bus), address(address)
    {}

    virtual std::string getVersion() = 0;
    virtual std::optional<HostState> getRequiredHostStateToGetVersion() = 0;

    virtual ~DeviceVersion() = default;
    DeviceVersion(const DeviceVersion&) = delete;
    DeviceVersion& operator=(const DeviceVersion&) = delete;
    DeviceVersion(DeviceVersion&&) = delete;
    DeviceVersion& operator=(DeviceVersion&&) = delete;

  protected:
    std::string chipModel;
    uint8_t bus;
    uint8_t address;
};

std::unique_ptr<DeviceVersion> getVersionProvider(
    const std::string& chipModel, const uint8_t& bus, const uint8_t& address);

} // namespace phosphor::software::eeprom
