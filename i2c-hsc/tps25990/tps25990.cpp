#include "tps25990.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::HSC
{

sdbusplus::async::task<bool> TPS25990::parseImage(const uint8_t* image,
                                                  size_t imageSize)
{
    std::string content(reinterpret_cast<const char*>(image), imageSize);
    std::istringstream imageStream(content);
    std::string line;
    constexpr std::string_view str_checksum = "CHECKSUM = ";

    configuration.clear();

    while (std::getline(imageStream, line))
    {
        if (line.empty() || line.find("/*") != std::string::npos ||
            line.find("RIMON") != std::string::npos)
        {
            continue;
        }

        if (line.starts_with(str_checksum))
        {
            configuration.checksum = static_cast<uint32_t>(
                std::stoul(line.substr(str_checksum.size())));
        }

        constexpr std::string_view delimiter = "h = 0x";
        auto pos = line.find(delimiter);

        if (pos != std::string::npos)
        {
            try
            {
                uint8_t reg = static_cast<uint8_t>(
                    std::stoul(std::string(line.substr(0, pos)), nullptr, 16));
                std::string valueStr =
                    std::string(line.substr(pos + delimiter.size()));
                size_t idx = 0;
                unsigned long raw_value = std::stoul(valueStr, &idx, 16);

                configuration.offsets.push_back(reg);

                if (idx == 2)
                {
                    uint8_t lo = static_cast<uint8_t>(raw_value & 0xFF);
                    configuration.data.push_back({lo});
                }
                else if (idx == 4)
                {
                    uint8_t lo = static_cast<uint8_t>(raw_value & 0xFF);
                    uint8_t hi = static_cast<uint8_t>((raw_value >> 8) & 0xFF);
                    configuration.data.push_back({lo, hi});
                }
                else
                {
                    error(
                        "parseImage failed. Invalid data length. Parsed length: {LENGTH}",
                        "LENGTH", std::to_string(idx));
                    co_return false;
                }
            }
            catch (const std::exception& e)
            {
                error("parseImage failed. Exception: {ERROR}", "ERROR",
                      e.what());
                co_return false;
            }
        }
    }

    if (configuration.offsets.size() != configuration.data.size())
    {
        error("parseImage failed. Data line mismatch.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::verifyImage(const uint8_t* image,
                                                   size_t imageSize)
{
    if (!co_await parseImage(image, imageSize))
    {
        error("Image verification failed: image parsing failed");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::unlockWriteProtect()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(regWriteProtect, UnlockWriteProtectData);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to unlock write protection");
        co_return false;
    }

    debug("Write protection unlocked");
    co_return true;
}

sdbusplus::async::task<bool> TPS25990::lockWriteProtect()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(regWriteProtect, LockWriteProtectData);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to lock write protection");
        co_return false;
    }

    debug("Write protection locked");
    co_return true;
}

sdbusplus::async::task<bool> TPS25990::importUserConfig()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    for (size_t i = 0; i < configuration.offsets.size(); ++i)
    {
        tbuf = buildByteVector(configuration.offsets[i]);
        tbuf.insert(tbuf.end(), configuration.data[i].begin(),
                    configuration.data[i].end());
        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("importUserConfig failed with sendreceive");
            co_return false;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::checkNVMavailable()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(regStatusMfrSpecific2);
    rbuf.resize(StatusMfrSpecific2Len);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to check NVM status");
        co_return false;
    }

    bool BankAvailable = rbuf[0] & ConfigNVMStatBit;
    if (!BankAvailable)
    {
        error("No bank is available in the NVM for programming");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::Program()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(regStoreUserAll);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to program the NVM with the revised configuration");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::getCRC(uint32_t* sum)
{
    if (!co_await getCheckSum(sum))
    {
        error("Failed to get CRC");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::checkProgram()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;
    uint32_t checksum = 0;

    if (!co_await getCheckSum(&checksum))
    {
        error("Failed to get checksum");
        co_return false;
    }

    if (checksum != configuration.checksum)
    {
        error("Programming checksum not match");
        co_return false;
    }

    tbuf = buildByteVector(regStatusCml);
    rbuf.resize(StatusCmlLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to check image programming result");
        co_return false;
    }

    bool ProgramFailed = rbuf[0] & ConfigNVMStatBit;
    if (ProgramFailed)
    {
        error("Image programming was unsuccessful");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::updateFirmware(bool force)
{
    (void)force;

    if (!co_await unlockWriteProtect())
    {
        co_return false;
    }

    if (!co_await importUserConfig())
    {
        co_return false;
    }

    if (!co_await checkNVMavailable())
    {
        co_return false;
    }

    if (!co_await Program())
    {
        co_return false;
    }

    if (!co_await lockWriteProtect())
    {
        co_return false;
    }

    if (!co_await checkProgram())
    {
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::getCheckSum(uint32_t* sum)
{
    uint8_t checksum = 0;

    for (const auto& data : TPS25990checksum_registers)
    {
        std::vector<uint8_t> tbuf{data};
        std::vector<uint8_t> rbuf(1);
        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("Failed to get checksum");
            co_return false;
        }

        // Bit 0 and Bit 4 of E1h and E2h are held by GPIO
        // and may not match the image values.
        // Use a mask to exclude Bit 0 and Bit 4 during verification.
        if (data == 0xE1 || data == 0xE2)
        {
            checksum ^= rbuf[0] & maskGPIOConfig;
        }
        else
        {
            checksum ^= rbuf[0];
        }

        for (int j = 0; j < 8; j++)
        {
            if (checksum & 0x80)
            {
                checksum = (checksum << 1) ^ 0x07;
            }
            else
            {
                checksum <<= 1;
            }
        }
    }

    *sum = checksum;

    co_return true;
}

} // namespace phosphor::software::HSC
