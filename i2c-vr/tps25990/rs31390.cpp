#include "rs31390.hpp"

#include "common/include/i2c/i2c.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{
constexpr uint8_t regCheckSum = 0xB5;
constexpr size_t checkSumLen = 2;

enum class RS31390Img : size_t
{
    commandCode = 2,
    sizeInBytes = 5,
    value = 6
};

enum Byte
{
    oneByte = 1,
    twoBytes = 2
};

sdbusplus::async::task<bool> RS31390::getCheckSum(uint32_t* sum)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(regCheckSum);
    rbuf.resize(checkSumLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to get checksum");
        co_return false;
    }

    *sum = bytesToInt<uint32_t>(rbuf);

    co_return true;
}

sdbusplus::async::task<bool> RS31390::parseImage(const uint8_t* image,
                                                 size_t imageSize)
{
    std::string content(reinterpret_cast<const char*>(image), imageSize);
    std::istringstream imageStream(content);
    std::string line;

    configuration.clear();

    while (std::getline(imageStream, line))
    {
        if (line.empty() || line.find("S.No") != std::string::npos)
        {
            continue;
        }

        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> cols;

        while (std::getline(ss, field, ','))
        {
            cols.push_back(field);
        }

        uint8_t reg = static_cast<uint8_t>(
            std::stoul(cols[static_cast<size_t>(RS31390Img::commandCode)], nullptr, 16));

        if (reg == regCheckSum)
        {
            configuration.checksum = static_cast<uint32_t>(
                std::stoul(cols[static_cast<size_t>(RS31390Img::value)], nullptr, 16));
            continue;
        }

        configuration.offsets.push_back(reg);

        uint16_t value = static_cast<uint16_t>(
            std::stoul(cols[static_cast<size_t>(RS31390Img::value)], nullptr, 16));

        if (std::stoul(cols[static_cast<size_t>(RS31390Img::sizeInBytes)], nullptr, 16) == oneByte)
        {
            uint8_t lo = static_cast<uint8_t>(value & 0xFF);
            configuration.data.push_back({lo});
        }
        else if (std::stoul(cols[static_cast<size_t>(RS31390Img::sizeInBytes)], nullptr, 16) ==
                 twoBytes)
        {
            uint8_t lo = static_cast<uint8_t>(value & 0xFF);
            uint8_t hi = static_cast<uint8_t>((value >> 8) & 0xFF);
            configuration.data.push_back({lo, hi});
        }
        else
        {
            error("parseImage failed. Data check failed.");
            co_return false;
        }
    }

    if (configuration.offsets.size() != configuration.data.size())
    {
        error("parseImage failed. Data line mismatch.");
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
