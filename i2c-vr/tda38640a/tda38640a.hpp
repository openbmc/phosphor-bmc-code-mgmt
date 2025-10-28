#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <unordered_set>

namespace phosphor::software::VR
{

class TDA38640A : public VoltageRegulator
{
  public:
    TDA38640A(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address);

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;

    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;

    bool forcedUpdateAllowed() final;

  private:
    struct Configuration
    {
        uint32_t rev;
        uint32_t checksum;
        std::vector<uint16_t> offsets;
        std::vector<std::vector<uint8_t>> data;

        void clear()
        {
            rev = 0;
            checksum = 0;
            offsets.clear();
            data.clear();
        }
    };

    sdbusplus::async::task<bool> getUserRemainingWrites(uint8_t* remain);
    sdbusplus::async::task<bool> getDeviceId(uint32_t* deviceId);
    sdbusplus::async::task<bool> program();
    sdbusplus::async::task<bool> getProgStatus(uint8_t* status);
    sdbusplus::async::task<bool> unlockDevice();
    sdbusplus::async::task<bool> programmingCmd();
    sdbusplus::async::task<bool> setPage(uint8_t page);
    sdbusplus::async::task<bool> getDeviceRevision(uint8_t* revision);

    bool parseImage(const uint8_t* image, size_t imageSize);

    phosphor::i2c::I2C i2cInterface;

    struct Configuration configuration;
};
} // namespace phosphor::software::VR
