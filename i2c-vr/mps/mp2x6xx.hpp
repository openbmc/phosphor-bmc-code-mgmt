#pragma once

#include "common/include/i2c/i2c.hpp"
#include "common/include/pmbus.hpp"
#include "i2c-vr/vr.hpp"

#include <array>

namespace phosphor::software::VR
{

struct MP2X6XXData;
struct MP2X6XXConfig;

class MP2X6XX : public VoltageRegulator
{
  public:
    MP2X6XX(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> reset() final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    bool forcedUpdateAllowed() final;
    std::set<RequestedApplyTimes> getAllowedApplyTimes() final
    {
        return {RequestedApplyTimes::OnReset};
    }

  private:
    std::map<uint8_t, std::vector<MP2X6XXData>> getGroupedConfigData();
    sdbusplus::async::task<bool> checkId(PMBusCmd pmBusCmd, uint32_t expected);
    sdbusplus::async::task<bool> unlockWriteProtect();
    sdbusplus::async::task<bool> storeUserCode();
    sdbusplus::async::task<bool> checkMTPCRC();
    sdbusplus::async::task<bool> selectConfig(uint8_t config);
    sdbusplus::async::task<bool> configAllRegisters();
    sdbusplus::async::task<bool> programConfigData(
        const std::vector<MP2X6XXData>& gdata);

    phosphor::i2c::I2C i2cInterface;
    std::shared_ptr<MP2X6XXConfig> configuration;
};

} // namespace phosphor::software::VR
