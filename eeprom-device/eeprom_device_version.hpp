#pragma once

#include "common/include/system_state.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace SystemStateInf = phosphor::software::system_state;

class DeviceVersion
{
  public:
    DeviceVersion(const std::string& chipModel, const uint16_t bus,
                  const uint8_t address) :
        chipModel(chipModel), bus(bus), address(address)
    {}

    virtual bool isDeviceReady()
    {
        return true;
    }
    virtual std::string getVersion() = 0;
    virtual std::optional<SystemStateInf::HostState>
        getHostStateToQueryVersion() = 0;
    virtual std::optional<SystemStateInf::OsState>
        getOsStateToQueryVersion() = 0;
    virtual ~DeviceVersion() = default;
    DeviceVersion(const DeviceVersion&) = delete;
    DeviceVersion& operator=(const DeviceVersion&) = delete;
    DeviceVersion(DeviceVersion&&) = delete;
    DeviceVersion& operator=(DeviceVersion&&) = delete;

  protected:
    std::string chipModel;
    uint16_t bus;
    uint8_t address;
};

std::unique_ptr<DeviceVersion> getVersionProvider(
    const std::string& chipModel, uint16_t bus, uint8_t address);
