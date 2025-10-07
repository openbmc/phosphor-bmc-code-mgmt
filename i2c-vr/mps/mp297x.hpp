#pragma once

#include "common/include/pmbus.hpp"
#include "mps.hpp"

namespace phosphor::software::VR
{

class MP297X : public MPSVoltageRegulator
{
  public:
    MP297X(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        MPSVoltageRegulator(ctx, bus, address)
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t& checksum) final;
    sdbusplus::async::task<bool> parseDeviceConfiguration() final;
    bool forcedUpdateAllowed() final;

  private:
    sdbusplus::async::task<bool> checkId(PMBusCmd pmBusCmd, uint32_t expected);
    sdbusplus::async::task<bool> isPasswordUnlock();
    sdbusplus::async::task<bool> unlockWriteProtect();
    sdbusplus::async::task<bool> storeDataIntoMTP();
    sdbusplus::async::task<bool> enableMTPPageWriteRead();
    sdbusplus::async::task<bool> enableMultiConfigCRC();
    sdbusplus::async::task<bool> checkMTPCRC();
    sdbusplus::async::task<bool> programPageRegisters(
        MPSPage page,
        const std::map<uint8_t, std::vector<MPSData>>& groupedData);
};

} // namespace phosphor::software::VR
