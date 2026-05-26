#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-hsc/hsc.hpp"

#include <sdbusplus/async.hpp>
#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <unordered_set>

namespace phosphor::software::HSC
{
constexpr uint8_t regWriteProtect = 0xF8;
constexpr uint8_t regStatusMfrSpecific2 = 0xF3;
constexpr uint8_t regStoreUserAll = 0x15;
constexpr uint8_t regStatusCml = 0x7E;
// Only RS31390 has CHECKSUM register.
constexpr uint8_t regCheckSum = 0xB5;

static constexpr size_t CheckSumLen = 2;
static constexpr size_t StatusMfrSpecific2Len = 2;
static constexpr size_t StatusCmlLen = 1;

constexpr uint8_t maskGPIOConfig = 0xEE;

constexpr uint8_t LockWriteProtectData = 0x00;
constexpr uint8_t UnlockWriteProtectData = 0xA2;
// regStatusMfrSpecific2 bit[0] == 0 mean 
// At least one bank is available in the NVM for programming.
constexpr uint8_t ConfigNVMStatBit = 0x01;
// regStatusCml bit[4] == 0 mean 
// STORE_USER_ALL command was successful.
constexpr uint8_t MemoryFltBit = (0x10 >> 4);

const std::array<uint8_t, 28> TPS25990checksum_registers = {
    0x58, 0x59, 0x57, 0x55, 0x43, 0x5F, 0x51, 0x4F, 0x6B, 0x5D,
    0xE0, 0xE1, 0xE2, 0xDB, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8,
    0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xF0, 0xF1, 0xF9
};

class TPS25990 : public HotSwapController
{
  public:
    TPS25990(sdbusplus::async::context& ctx, uint16_t bus,
                        uint16_t address) :
        HotSwapController(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
    {}

    virtual sdbusplus::async::task<bool> getCheckSum(uint32_t* sum);
    virtual sdbusplus::async::task<bool> parseImage(const uint8_t* image,
                                                    size_t imageSize);

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* sum) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;

  private:
    sdbusplus::async::task<bool> unlockWriteProtect();
    sdbusplus::async::task<bool> lockWriteProtect();
    sdbusplus::async::task<bool> importUserConfig();
    sdbusplus::async::task<bool> checkNVMavailable();
    sdbusplus::async::task<bool> Program();
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

} // namespace phosphor::software::HSC