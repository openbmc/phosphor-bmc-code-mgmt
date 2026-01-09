#include "xdp71x.hpp"

#include "common/include/i2c/i2c.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

const uint16_t CRC16_CCITT_POLY = 0x1021;

enum class XDP71XCmd : uint8_t
{
    mfrRevisionReg = 0x9B,
    statusMemReg = 0xEB
};

struct regData
{
    uint8_t reg;
    uint8_t readSize;
};

const std::vector<regData> pmbusCfgRegs = {
    {0x01, 1}, {0x42, 2}, {0x43, 2}, {0x44, 2}, {0x4A, 2}, {0x4F, 2},
    {0x51, 2}, {0x55, 2}, {0x57, 2}, {0x58, 2}, {0x59, 2}, {0x6B, 2},
    {0xD0, 1}, {0xD1, 1}, {0xD3, 2}, {0xD4, 2}, {0xD5, 2}, {0xD6, 2},
    {0xD7, 2}, {0xD8, 2}, {0xD9, 2}, {0xDA, 1}, {0xDB, 2}, {0xDC, 2},
    {0xDD, 1}, {0xDE, 2}, {0xDF, 2}, {0xE1, 2}, {0xE2, 2}, {0xE4, 1},
    {0xE5, 2}, {0xE6, 2}, {0xE7, 2}, {0xE8, 2}, {0xE9, 2}};

XDP71X::XDP71X(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
{}

uint16_t XDP71X::crc16_ccitt(const std::vector<uint8_t>& hex_numbers)
{
    uint16_t crc = 0xFFFF;

    for (auto& data : hex_numbers)
    {
        crc ^= data << 8;
        for (int i = 0; i < 8; i++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ CRC16_CCITT_POLY;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool XDP71X::getRevision(uint8_t* revision)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(XDP71XCmd::mfrRevisionReg);
    rbuf.resize(2);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("getRevision failed with sendreceive");
        return false;
    }

    *revision = rbuf[0];

    return true;
}

sdbusplus::async::task<bool> XDP71X::getCRC(uint32_t* sum)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;
    std::vector<uint8_t> results;

    uint8_t revision;
    if (!getRevision(&revision))
    {
        error("getCRC failed with getRevision");
        co_return false;
    }
    debug("XDP Revision is : {REV}", "REV", lg2::dec, revision);

    for (auto& cfgReg : pmbusCfgRegs)
    {
        tbuf = buildByteVector(cfgReg.reg);

        // XDP710 is byte
        if (cfgReg.reg == 0xD8 && revision == 1)
        {
            rbuf.resize(1);
        }
        else
        {
            rbuf.resize(cfgReg.readSize);
        }

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("getCRC failed with sendreceive pmbusCfgReg: {REG}", "REG",
                  lg2::dec, cfgReg.reg);
            co_return false;
        }

        for (auto& readData : rbuf)
        {
            results.emplace_back(readData);
        }
    }

    *sum = crc16_ccitt(results);

    co_return true;
}

bool XDP71X::forcedUpdateAllowed()
{
    return false;
}

sdbusplus::async::task<bool> XDP71X::verifyImage(const uint8_t* image,
                                                 size_t imageSize)
{
    (void)image;
    (void)imageSize;
    error("XDP71X not support updateFirmware currently");
    co_return false;
}
sdbusplus::async::task<bool> XDP71X::updateFirmware(bool force)
{
    (void)force;
    error("XDP71X not support updateFirmware currently");
    co_return false;
}

} // namespace phosphor::software::VR
