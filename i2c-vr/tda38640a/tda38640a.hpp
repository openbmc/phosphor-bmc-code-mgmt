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

    std::unordered_set<uint16_t> user_section_otp_register{
        0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048,
        0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051,
        0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A,
        0x005B, 0x005C, 0x005D, 0x005E, 0x005F, 0x0060, 0x0061, 0x0062, 0x0063,
        0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C,
        0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075,
        0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x0202, 0x0204, 0x0220,
        0x0240, 0x0242, 0x0243, 0x0248, 0x0249, 0x024A, 0x024B, 0x024C, 0x024D,
        0x024E, 0x024F, 0x0250, 0x0251, 0x0252, 0x0256, 0x0257, 0x0266, 0x0267,
        0x026A, 0x026C, 0x0270, 0x0272, 0x0273, 0x0280, 0x0281, 0x0282, 0x0288,
        0x0289, 0x028A, 0x028C, 0x028D, 0x028E, 0x029E, 0x02A0, 0x02A2, 0x02AA,
        0x02AB, 0x02AC, 0x02BC, 0x02BD, 0x02BE, 0x02BF, 0x02C0, 0x02C2, 0x02C8,
        0x02CA, 0x0384, 0x0385};

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
