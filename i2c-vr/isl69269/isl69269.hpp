#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>

namespace phosphor::software::VR
{

    class ISL69269 : public VoltageRegulator
    {
        public:
            ISL69269(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address);

            sdbusplus::async::task<bool> verifyImage(const uint8_t* image, size_t imageSize) final;

            sdbusplus::async::task<bool> updateFirmware(bool force) final;
            sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
            sdbusplus::async::task<bool> reset() final;

            bool forcedUpdateAllowed() final;

        private:
            struct d
            {
                uint8_t addr;
                uint8_t cmd;
                uint8_t* data;

            } __attribute__((packed));

            struct raaData
            {
                union {
                    uint8_t raw[32];
                    struct d notRaw;
                };
                uint8_t len;
                uint8_t pec;
            };

            struct raaConfig
            {
                uint8_t addr;
                uint8_t mode;
                uint8_t cfgId;
                uint16_t wrCnt;
                uint32_t devIdExp;
                uint32_t devRevExp;
                uint32_t crcExp;
                struct raaData pData[1024];
            };
            sdbusplus::async::task<bool> dmaReadWrite(uint8_t* reg, uint8_t* resp);
            sdbusplus::async::task<bool> getRemainingWrites(uint8_t* remain);
            sdbusplus::async::task<bool> getHexMode(uint8_t* mode);
            sdbusplus::async::task<bool> getDeviceId(uint32_t* deviceId);
            sdbusplus::async::task<bool> parseImage(const uint8_t* image, size_t imageSize);

            phosphor::i2c::I2C i2cInterface;
    };
}
