#include "isl69269.hpp"

#include "common/include/i2c/i2c.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

const uint8_t RAARegCRC = 0x94;
const uint8_t RAARegDMAData = 0xC5;
const uint8_t RAARegDMAAddr = 0xC7;

const uint8_t RAARegRemainginWrites = 0x35;

const uint8_t RAADeviceIdLength = 4;
const uint8_t RAARegHexModeCFG0 = 0x87;
const uint8_t RAARegHexModeCFG1 = 0xBD;

const uint8_t RAAGen2 = 0;
const uint8_t RAAGen3Legacy = 1;
const uint8_t RAAGen3Production = 2;

// Common PMBus Command codes
const uint8_t PMBusDeviceId = 0xAD;

// Config file constants
const char RecordTypeHeader = 0x49;

ISL69269::ISL69269(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
    VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
{}

sdbusplus::async::task<bool> ISL69269::dmaReadWrite(uint8_t* reg, uint8_t* resp)
{
    bool ret = false;
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegDMAData;
    std::memcpy(&tbuf[1],reg, 2);

    ret = co_await this->i2cInterface.sendReceive(tbuf,3,rbuf,0);
    if (!ret)
    {
        error("dmaReadWrite failed with {CMD}", "CMD", std::string("RAA_REG_DMA_ADDR"));
        co_return false;
    }

    tbuf[0] = RAARegDMAAddr;
    ret = co_await this->i2cInterface.sendReceive(tbuf,1, resp, 4);
    if (!ret)
    {
        error("dmaReadWrite failed with {CMD}", "CMD", std::string("RAA_REG_DMA_DATA"));
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getRemainingWrites(uint8_t* remain)
{
    bool ret = false;
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegRemainginWrites;
    tbuf[1] = 0x00;
    ret = co_await dmaReadWrite(tbuf, rbuf);
    if (!ret)
    {
        error("getRemainingWrites failed");
        co_return false;
    }

    *remain = rbuf[0];
    co_return true;
}

sdbusplus::async::task<bool> ISL692969::getHexMode(uint8_t* mode)
{
    bool ret = false;
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegHexModeCFG0;
    tbuf[1] = RAARegHexModeCFG1;
    ret = co_await this->dmaReadWrite(tbuf, rbuf);
    if (!ret)
    {
        error("getHexMode failed");
        co_return false;
    }

    *mode = (rbuf[0] == 0) ? RAAGen3Legacy : RAAGen3Production;

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getDeviceId(uint8_t* deviceId)
{
    bool ret = false;
    uint8_t tbuf[16] = {0};
    uint8_t tLen = 1;
    uint8_t rbuf[16] = {0};
    uint8_t rLen = RAADeviceIdLength + 1;

    tbuf[0] = PMBusDeviceId;

    ret = co_await this->i2cInterface.sendReceive(tbuf,rLen ,rbuf, tLen);
    if (!ret)
    {
        error("getDeviceId failed");
        co_return false;
    }

    *deviceId = rbuf[1];

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::getCRC(uint32_t* sum)
{
    bool ret = false;
    uint8_t tbuf[16] = {0};
    uint8_t rbuf[16] = {0};

    tbuf[0] = RAARegCRC;
    ret = co_await this->dmaReadWrite(tbuf, rbuf);
    if (!ret)
    {
        error("getCRC failed");
        co_return false;
    }
    std::memcpy(sum, rbuf, sizeof(uint32_t));

    co_return true;
}

sdbusplus::async::task<bool> ISL69269::parseImage(uint8_t* image, size_t imageSize)
{
    size_t lineNo = 0;
    size_t nextLineStart = 0;
    char line[40];

    for (size_t i = 0; i < imageSize; i++)
    {
        if (image[i] == '\n') // We have a hex file, so we check new line.
        {
            size_t lineLen = i - nextLineStart;
            std::memcpy(line, image + nextLineStart, lineLen);
            for (size_t j = 0; j < lineLen; j++)
            {
                if (lineNo <=3 && line[j] == RecordTypeHeader)
                {
                    // First four lines can be header lines
                }

            }
            lineNo++;
            nextLineStart += i+1;
        }
    }
}

}
