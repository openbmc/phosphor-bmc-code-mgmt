#include "tps25990.hpp"

#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{
constexpr uint8_t regWriteProtect = 0xF8;
constexpr uint8_t regStatusMfrSpecific2 = 0xF3;
constexpr uint8_t regStoreUserAll = 0x15;
constexpr uint8_t regStatusCml = 0x7E;
constexpr uint8_t regGPIOConfig12 = 0xE1;
constexpr uint8_t regGPIOConfig34 = 0xE2;
constexpr uint8_t maskGPIOConfig = 0xEE;
constexpr uint8_t lockWriteProtectData = 0x00;
constexpr uint8_t unlockWriteProtectData = 0xA2;
constexpr uint8_t configNVMStatBit = 0x01;
constexpr uint8_t memoryFltBit = 0x10;

constexpr size_t statusMfrSpecific2Len = 2;
constexpr size_t statusCmlLen = 1;
constexpr size_t hexDigitPerByte = 2;
constexpr size_t hexDigitPerTwoByte = 4;

constexpr std::chrono::milliseconds storeOperationLatency{500};

constexpr std::array<uint8_t, 28> TPS25990checksum_registers = {
    0x58, 0x59, 0x57, 0x55, 0x43, 0x5F, 0x51, 0x4F, 0x6B, 0x5D,
    0xE0, 0xE1, 0xE2, 0xDB, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8,
    0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xF0, 0xF1, 0xF9};

sdbusplus::async::task<bool> TPS25990::parseImage(const uint8_t* image,
                                                  size_t imageSize)
{
    std::string content(reinterpret_cast<const char*>(image), imageSize);
    std::istringstream imageStream(content);
    std::string line;
    constexpr std::string_view checksumPrefix = "CHECKSUM = ";
    constexpr std::string_view blockComment = "/*";
    constexpr std::string_view rimonPrefix = "RIMON";
    constexpr std::string_view delimiter = "h = 0x";

    configuration.clear();

    while (std::getline(imageStream, line))
    {
        if (line.empty() || line.find(blockComment) != std::string::npos ||
            line.find(rimonPrefix) != std::string::npos)
        {
            continue;
        }

        if (line.starts_with(checksumPrefix))
        {
            configuration.checksum = static_cast<uint32_t>(
                std::stoul(line.substr(checksumPrefix.size())));
        }

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

                if (idx == hexDigitPerByte)
                {
                    uint8_t lo = static_cast<uint8_t>(raw_value & 0xFF);
                    configuration.data.push_back({lo});
                }
                else if (idx == hexDigitPerTwoByte)
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

    tbuf = buildByteVector(regWriteProtect, unlockWriteProtectData);
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

    tbuf = buildByteVector(regWriteProtect, lockWriteProtectData);
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
    rbuf.resize(statusMfrSpecific2Len);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to check NVM status");
        co_return false;
    }

    // regStatusMfrSpecific2 bit[0] == 0 mean
    // At least one bank is available in the NVM for programming.
    bool bankNotAvailable = rbuf[0] & configNVMStatBit;
    if (bankNotAvailable)
    {
        error("No bank is available in the NVM for programming");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS25990::program()
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(regStoreUserAll);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to program the NVM with the revised configuration");
        co_return false;
    }

    // Wait store user code
    co_await sdbusplus::async::sleep_for(ctx, storeOperationLatency);

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
    rbuf.resize(statusCmlLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("Failed to check image programming result");
        co_return false;
    }

    // regStatusCml bit[4] == 0 mean
    // STORE_USER_ALL command was successful.
    bool programFailed = rbuf[0] & memoryFltBit;
    if (programFailed)
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

    if (!co_await program())
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

        // Bit 0 and Bit 4 of regGPIOConfig12 and regGPIOConfig34
        // are held by GPIO and may not match the image values.
        // Use a mask to exclude Bit 0 and Bit 4 during verification.
        if (data == regGPIOConfig12 || data == regGPIOConfig34)
        {
            checksum ^= rbuf[0] & maskGPIOConfig;
        }
        else
        {
            checksum ^= rbuf[0];
        }

        // Calculate CRC-8 checksum.
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

bool TPS25990::forcedUpdateAllowed()
{
    return true;
}

} // namespace phosphor::software::VR
