#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <unordered_set>

namespace phosphor::software::VR
{

class XDP71X : public VoltageRegulator
{
  public:
    XDP71X(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address);

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;

    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;

    bool forcedUpdateAllowed() final;

  private:
    phosphor::i2c::I2C i2cInterface;

    static uint16_t crc16_ccitt(const std::vector<uint8_t>& hex_numbers);
    bool getRevision(uint8_t* revision);
};
} // namespace phosphor::software::VR
