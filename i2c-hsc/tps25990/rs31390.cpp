#include "rs31390.hpp"

#include "common/include/i2c/i2c.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::HSC
{

sdbusplus::async::task<bool> RS31390::getCheckSum(uint32_t* sum)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(regCheckSum);
    rbuf.resize(CheckSumLen);
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
        if (line.empty() || line.find("S.no") != std::string::npos)
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

        uint8_t reg = static_cast<uint8_t>(std::stoul(cols[2], nullptr, 16));

        if (reg == regCheckSum)
        {
            configuration.checksum = static_cast<uint32_t>(std::stoul(cols[6]));
            continue;
        }

        configuration.offsets.push_back(reg);

        uint16_t value =
            static_cast<uint16_t>(std::stoul(cols[6], nullptr, 16));


        if (std::stoul(cols[5], nullptr, 16) == 1)
        {
            uint8_t lo = static_cast<uint8_t>(value & 0xFF);
            configuration.data.push_back({lo});
        }
        else if (std::stoul(cols[5], nullptr, 16) == 2)
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

} // namespace phosphor::software::HSC