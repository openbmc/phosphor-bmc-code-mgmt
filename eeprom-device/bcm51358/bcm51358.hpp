#pragma once

#include "eeprom-device/eeprom_device_version.hpp"
#include "serial/serial_terminal.hpp"

using namespace phosphor::software::serial;

class BCM51358DeviceVersion : public DeviceVersion, public SerialTerminal
{
  public:
    BCM51358DeviceVersion(const std::string& chipModel, const std::string& port,
                          const uint32_t baud) :
        DeviceVersion(chipModel, 0, 0),
        SerialTerminal(chipModel, port, baud, "CMD> ")
    {}

    std::string getVersion() final;
    std::optional<HostPowerInf::HostState> getHostStateToQueryVersion() final
    {
        return std::nullopt;
    }
};
