#pragma once

#include "mps.hpp"

#include <array>

namespace phosphor::software::VR
{

struct MP2X6XXData
{
    uint8_t page = 0;
    uint8_t addr = 0;
    std::array<uint8_t, 4> data{};
    uint8_t length = 0;
};

struct MP2X6XXConfig
{
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint32_t configId = 0;
    uint32_t crc = 0;
    std::vector<MP2X6XXData> pdata;
};

class MP2X6XX : public MPSVoltageRegulator
{
  public:
    MP2X6XX(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address)
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> reset() final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    bool forcedUpdateAllowed() final;

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

    struct MP2X6XXConfig config;
};

} // namespace phosphor::software::VR
