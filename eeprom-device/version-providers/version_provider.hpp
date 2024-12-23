#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace phosphor::software::eeprom::version
{

class VersionProvider
{
  public:
    VersionProvider(const std::string& chipModel, const uint8_t& bus,
                    const uint8_t& address) :
        chipModel(chipModel), bus(bus), address(address)
    {}

    virtual std::string getVersion() = 0;
    virtual bool isHostOnRequiredToGetVersion() = 0;

    virtual ~VersionProvider() = default;
    VersionProvider(const VersionProvider&) = delete;
    VersionProvider& operator=(const VersionProvider&) = delete;
    VersionProvider(VersionProvider&&) = delete;
    VersionProvider& operator=(VersionProvider&&) = delete;

  protected:
    std::string chipModel;
    uint8_t bus;
    uint8_t address;
};

std::unique_ptr<VersionProvider> getVersionProvider(
    const std::string& chipModel, const uint8_t& bus, const uint8_t& address);

} // namespace phosphor::software::eeprom::version
