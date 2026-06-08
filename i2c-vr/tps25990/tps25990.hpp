#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <unordered_set>

namespace phosphor::software::VR
{

class TPS25990 : public VoltageRegulator
{
  public:
    TPS25990(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
    {}

    virtual sdbusplus::async::task<bool> getCheckSum(uint32_t* sum);
    virtual sdbusplus::async::task<bool> parseImage(const uint8_t* image,
                                                    size_t imageSize);

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* sum) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    bool forcedUpdateAllowed() final;

  private:
    sdbusplus::async::task<bool> unlockWriteProtect();
    sdbusplus::async::task<bool> lockWriteProtect();
    sdbusplus::async::task<bool> importUserConfig();
    sdbusplus::async::task<bool> checkNVMavailable();
    sdbusplus::async::task<bool> program();
    sdbusplus::async::task<bool> checkProgram();

  protected:
    struct Configuration
    {
        uint32_t checksum = 0;
        std::vector<uint8_t> offsets;
        std::vector<std::vector<uint8_t>> data;

        void clear()
        {
            checksum = 0;
            offsets.clear();
            data.clear();
        }
    };

    struct Configuration configuration;
    phosphor::i2c::I2C i2cInterface;
};

} // namespace phosphor::software::VR
