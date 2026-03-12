#include "lattice_xo5_cpld.hpp"

#include <openssl/sha.h>

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

constexpr std::chrono::milliseconds readyPollInterval(10);
constexpr std::chrono::milliseconds tSeriesReadyPollInterval(1);
constexpr std::chrono::milliseconds readyTimeout(1000);
constexpr std::chrono::milliseconds eraseTimeout(20000);

enum class xo5Cmd : uint8_t
{
    sectorErase = 0xd8,
    pageProgram = 0x02,
    pageRead = 0x0b,
    readUsercode = 0xc0,
    noop = 0xff,
    noop0 = 0x00,
    readDeviceID = 0xe0,
    readSoftIpId = 0xe6,
    readStatusReg = 0x3c,
    checkBusyStatus = 0xf0,
    enablex = 0x74,
    disable = 0x26,
    initAddress = 0x46,
    erase = 0x0e,
    programIncr = 0x82,
    readIncr = 0x6a,
    programDone = 0x5e,
    refresh = 0x79,
    calcHash = 0x7c,
    readHash = 0xe5,
    controlCmdCrc = 0xfd,
    lock = 0xa2
};

enum class xo5Status : uint8_t
{
    ready = 0x00,
    notReady = 0xff
};

struct xo5Cfg
{
    static constexpr size_t pageSize = 256;
    static constexpr size_t pagesPerBlock = 256;
    static constexpr size_t blocksPerCfg = 11;

    static constexpr size_t incrDataSize = 128;
    static constexpr size_t retryMax = 3;
    static constexpr uint8_t headerIdx = 0;
    static constexpr uint8_t cfg0Idx = 1;
    static constexpr uint8_t cfg1Idx = 2;
    static constexpr uint8_t ufm8Idx = 15;
};

static bool getStartBlock(uint8_t cfg, uint8_t& startBlock)
{
    static constexpr std::array<uint8_t, 3> cfgStartBlocks = {0x01, 0x10, 0x1F};

    if (cfg >= cfgStartBlocks.size())
    {
        return false;
    }

    startBlock = cfgStartBlocks[cfg];
    return true;
}

static uint16_t crc16Ccitt(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint16_t appendCrc16(std::vector<uint8_t>& data)
{
    uint16_t crc = crc16Ccitt(&data[0], data.size());
    data.push_back(static_cast<uint8_t>(crc & 0xFF));
    data.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    return crc;
}

std::optional<std::vector<uint8_t>> LatticeXO5CPLD::calculateSha2_384Openssl(
    const std::vector<uint8_t>& input)
{
    std::vector<uint8_t> digest(SHA384_DIGEST_LENGTH);
    if (SHA384(input.data(), input.size(), digest.data()) == nullptr)
    {
        lg2::error("Failed to compute SHA2-384 digest using OpenSSL SHA384()");
        return std::nullopt;
    }
    return digest;
}

uint8_t LatticeXO5CPLD::getCfgIdx(std::string_view target) const
{
    if (target.empty())
    {
        return 0;
    }
    std::string lowerTarget(target);
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
                   ::tolower);

    uint8_t baseIdx = (lowerTarget == "cfg1") ? 1 : 0;

    if (isTSeries)
    {
        return baseIdx + 1;
    }
    return baseIdx;
}

LatticeXO5CPLD::LatticeXO5CPLD(
    sdbusplus::async::context& ctx, const uint16_t bus, const uint8_t address,
    const std::string& chip, const std::string& target, latticeChip chipType,
    const bool debugMode) :
    LatticeBaseCPLD(ctx, bus, address, chip, target, debugMode),
    isTSeries(chipType == latticeChip::LFMXO5_35T ||
              chipType == latticeChip::LFMXO5_65T)
{
    std::string modeStr = isTSeries ? "T-Series" : "Standard";
    lg2::info("Lattice XO5 Driver loaded. Mode: {MODE}", "MODE", modeStr);
}

#define AUTO_LOCK_I2C                                                          \
    auto lockI2CResult = co_await lockI2C();                                   \
    if (!lockI2CResult)                                                        \
    {                                                                          \
        lg2::error("Failed to acquire I2C lock.");                             \
        co_return false;                                                       \
    }

sdbusplus::async::task<bool> LatticeXO5CPLD::lockI2C()
{
    if ((softIpVersion & 0xF0) != 0x20)
    {
        co_return true;
    }
    std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::lock), 0x01};
    std::vector<uint8_t> response = {0x00};
    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send lock command.");
        co_return false;
    }

    if (!response.empty() && (response[0] & 0xFE) != 0x00)
    {
        lg2::debug("I2C lock acquired");
        co_return true;
    }

    lg2::error("I2C lock not acquired");
    co_return false;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::waitUntilReady(
    std::chrono::milliseconds timeout)
{
    const auto endTime = std::chrono::steady_clock::now() + timeout;
    const auto pollInterval =
        isTSeries ? tSeriesReadyPollInterval : readyPollInterval;

    auto readDummy = [this]() -> sdbusplus::async::task<bool> {
        std::vector<uint8_t> request;
        if (this->isTSeries)
        {
            AUTO_LOCK_I2C;
            request = {static_cast<uint8_t>(xo5Cmd::checkBusyStatus), 0x00,
                       0x00, 0x00};
        }
        else
        {
            request = {};
        }
        std::vector<uint8_t> response = {0xff};
        if (!i2cInterface.sendReceive(request, response))
        {
            lg2::error("Failed to read.");
            co_return false;
        }
        if (response.at(0) == static_cast<uint8_t>(xo5Status::ready))
        {
            co_return true;
        }
        co_return false;
    };

    while (std::chrono::steady_clock::now() < endTime)
    {
        if (co_await readDummy())
        {
            co_return true;
        }
        co_await sdbusplus::async::sleep_for(ctx, pollInterval);
    }

    lg2::error("Timeout waiting for device ready");
    co_return false;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::toggleCrc16(bool enable)
{
    AUTO_LOCK_I2C;
    std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::controlCmdCrc),
                                    static_cast<uint8_t>(enable ? 0x01 : 0x00),
                                    0x00, 0x00};
    std::vector<uint8_t> response = {};

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to toggle CRC16.");
        co_return false;
    }
    crc16Enabled_ = enable;
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::readCrcStatus(bool& crc_ok)
{
    std::vector<uint8_t> request = {
        static_cast<uint8_t>(xo5Cmd::checkBusyStatus), 0x00, 0x00, 0x00};
    std::vector<uint8_t> response(1, 0xFF);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to read CRC status.");
        co_return false;
    }

    crc_ok = (response.at(0) & 0x02) == 0;
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::sendReceive(
    const std::vector<uint8_t>& request, std::vector<uint8_t>& response)
{
    if (request.empty())
    {
        lg2::error("Error: request vector is empty.");
        co_return false;
    }

    bool crc16Enabled = false;
    uint8_t opcode = request.at(0);
    if ((opcode == static_cast<uint8_t>(xo5Cmd::programIncr)) ||
        (opcode == static_cast<uint8_t>(xo5Cmd::readIncr)) ||
        (opcode == static_cast<uint8_t>(xo5Cmd::readSoftIpId)))
    {
        crc16Enabled = true;
    }
    else if ((opcode == static_cast<uint8_t>(xo5Cmd::controlCmdCrc)) ||
             (opcode == static_cast<uint8_t>(xo5Cmd::checkBusyStatus)) ||
             (opcode == static_cast<uint8_t>(xo5Cmd::readStatusReg)) ||
             (opcode == static_cast<uint8_t>(xo5Cmd::refresh)))
    {
        crc16Enabled = false;
    }
    else if (((softIpVersion & 0xF0) == 0x20) &&
             ((opcode == static_cast<uint8_t>(xo5Cmd::refresh)) ||
              (opcode == static_cast<uint8_t>(xo5Cmd::readDeviceID)) ||
              (opcode == static_cast<uint8_t>(xo5Cmd::readSoftIpId)) ||
              (opcode == static_cast<uint8_t>(xo5Cmd::readUsercode)) ||
              (opcode == static_cast<uint8_t>(xo5Cmd::lock))))
    {
        crc16Enabled = false;
    }
    else
    {
        crc16Enabled = crc16Enabled_;
    }

    if (!crc16Enabled)
    {
        co_return i2cInterface.sendReceive(request, response);
    }

    std::vector<uint8_t> request_crc = request;
    appendCrc16(request_crc);
    std::vector<uint8_t> response_crc = response;
    if (!response_crc.empty() &&
        (opcode != static_cast<uint8_t>(xo5Cmd::programIncr)))
    {
        response_crc.push_back(0x00);
        response_crc.push_back(0x00);
    }

    std::size_t j = 0;
    for (; j < xo5Cfg::retryMax; ++j)
    {
        if (!i2cInterface.sendReceive(request_crc, response_crc))
        {
            lg2::error("Failed to sendReceive with CRC16.");
            co_return false;
        }
        bool crc_ok = false;
        if (opcode == static_cast<uint8_t>(xo5Cmd::programIncr))
        {
            crc_ok =
                (!response_crc.empty() && (response_crc[0] & 0x01) == 0x01);
        }
        else if (!(co_await readCrcStatus(crc_ok)))
        {
            lg2::error("Failed to read CRC status.");
            co_return false;
        }
        if (crc_ok)
        {
            if (!response.empty() &&
                (response_crc.size() == response.size() + 2))
            {
                std::copy(response_crc.begin(), response_crc.end() - 2,
                          response.begin());
            }
            co_return true;
        }
        else
        {
            lg2::warning("CRC16 check failed, retrying... Attempt {ATTEMPT}",
                         "ATTEMPT", j + 1);
            continue;
        }
    }

    if (j == xo5Cfg::retryMax)
    {
        lg2::error(
            "Failed to sendReceive with CRC16 after {MAX_RETRIES} attempts.",
            "MAX_RETRIES", xo5Cfg::retryMax);
    }
    co_return false;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::eraseCfg(
    std::optional<uint8_t> setIdx)
{
    uint8_t cfgIndex = setIdx.has_value() ? setIdx.value() : getCfgIdx(target);
    std::vector<uint8_t> response = {};
    if (isTSeries)
    {
        AUTO_LOCK_I2C;
        std::vector<std::vector<uint8_t>> cmds = {
            {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
            {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex, 0x0},
            {static_cast<uint8_t>(xo5Cmd::erase), 0x0, cfgIndex, 0x0}};

        for (const auto& cmd : cmds)
        {
            if (!(co_await sendReceive(cmd, response)))
            {
                lg2::error("Failed to send command");
                co_return false;
            }
        }

        if (!(co_await waitUntilReady(eraseTimeout)))
        {
            lg2::error("Failed to wait until ready");
            co_return false;
        }

        if (!(co_await sendReceive(
                {static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0},
                response)))
        {
            lg2::error("Failed to disable command");
            co_return false;
        }
    }
    else
    {
        uint8_t startBlock;
        if (!getStartBlock(cfgIndex, startBlock))
        {
            lg2::error("Error: invalid cfg index.");
            co_return false;
        }
        const auto endBlock = startBlock + xo5Cfg::blocksPerCfg;

        auto eraseBlock =
            [this, &response](uint8_t block) -> sdbusplus::async::task<bool> {
            std::vector<uint8_t> request;
            request.reserve(4);
            request.push_back(static_cast<uint8_t>(xo5Cmd::sectorErase));
            request.push_back(block);
            request.push_back(0x0);
            request.push_back(0x0);
            if (!i2cInterface.sendReceive(request, response))
            {
                lg2::error("Failed to erase block");
                co_return false;
            }
            co_return true;
        };

        for (size_t block = startBlock; block < endBlock; ++block)
        {
            if (!(co_await eraseBlock(block)))
            {
                lg2::error("Erase failed: Block {BLOCK}", "BLOCK", block);
                co_return false;
            }
            if (!(co_await waitUntilReady(readyTimeout)))
            {
                lg2::error("Failed to wait until ready");
                co_return false;
            }
        }
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::programPage(
    uint8_t block, uint8_t page, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> request;
    std::vector<uint8_t> response = {};
    request.reserve(4 + data.size());
    request.push_back(static_cast<uint8_t>(xo5Cmd::pageProgram));
    request.push_back(block);
    request.push_back(page);
    request.push_back(0x0);
    request.insert(request.end(), data.begin(), data.end());

    if (!i2cInterface.sendReceive(request, response))
    {
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::programCfg(
    std::optional<uint8_t> setIdx, const std::vector<uint8_t>* customData)
{
    using diff_t = std::vector<uint8_t>::difference_type;
    uint8_t cfgIndex = setIdx.has_value() ? setIdx.value() : getCfgIdx(target);
    std::vector<uint8_t> cfgData =
        (customData != nullptr) ? *customData : fwInfo.cfgData;
    size_t totalBytes = cfgData.size();

    if (isTSeries)
    {
        AUTO_LOCK_I2C;
        std::vector<uint8_t> emptyResponse = {};
        std::vector<uint8_t> response;
        std::vector<std::vector<uint8_t>> preWriteCmds = {
            {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
            {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex, 0x0},
            {static_cast<uint8_t>(xo5Cmd::calcHash), 0x0, 0x0, 0x0}};

        for (const auto& cmd : preWriteCmds)
        {
            if (!(co_await sendReceive(cmd, emptyResponse)))
            {
                lg2::error("Failed to send command opcode: {OPCODE}", "OPCODE",
                           lg2::hex, cmd[0]);
                co_return false;
            }
        }
        lg2::debug("Pre-programming completed successfully");

        const auto chunkSize = xo5Cfg::incrDataSize;
        if (totalBytes % chunkSize != 0)
        {
            cfgData.insert(cfgData.end(), chunkSize - (totalBytes % chunkSize),
                           0xFF);
            totalBytes = cfgData.size();
        }

        unsigned int i = 0;
        for (size_t offset = 0; offset < totalBytes; offset += chunkSize)
        {
            std::vector<uint8_t> chunk = {
                static_cast<uint8_t>(xo5Cmd::programIncr), 0x0, 0x0, 0x0};
            auto startIdx = static_cast<std::ptrdiff_t>(offset);
            auto endIdx = static_cast<std::ptrdiff_t>(offset + chunkSize);

            chunk.insert(chunk.end(), std::next(cfgData.begin(), startIdx),
                         std::next(cfgData.begin(), endIdx));
            response = {0xFF};
            bool success = co_await sendReceive(chunk, response);
            if (i % 2 == 0)
            {
                success &= co_await waitUntilReady(readyTimeout);
            }
            if (!success)
            {
                lg2::error("Failed to program incr");
                co_return false;
            }
        }
        lg2::debug("Programming data completed successfully");

        std::vector<std::vector<uint8_t>> postWriteCmds = {
            {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x00, 0x0},
            {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex, 0x0},
            {static_cast<uint8_t>(xo5Cmd::programDone), 0x0, 0x0, 0x0},
            {static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0}};
        for (const auto& cmd : postWriteCmds)
        {
            if (!(co_await sendReceive(cmd, emptyResponse)))
            {
                lg2::error("Failed to send command opcode: {OPCODE:X}",
                           "OPCODE", lg2::hex, cmd[0]);
                co_return false;
            }
        }
        lg2::debug("Post-programming completed successfully");
    }
    else
    {
        uint8_t startBlock;
        if (!getStartBlock(cfgIndex, startBlock))
        {
            lg2::error("Error: invalid cfg index.");
            co_return false;
        }
        const auto endBlock = startBlock + xo5Cfg::blocksPerCfg;
        size_t bytesWritten = 0;

        for (size_t block = startBlock; block < endBlock; ++block)
        {
            for (size_t page = 0; page < xo5Cfg::pagesPerBlock; ++page)
            {
                if (bytesWritten >= totalBytes)
                {
                    co_return true;
                }

                auto offset = static_cast<diff_t>(bytesWritten);
                auto remaining = static_cast<diff_t>(totalBytes - bytesWritten);
                const auto chunkSize =
                    std::min(static_cast<diff_t>(xo5Cfg::pageSize), remaining);
                std::vector<uint8_t> chunk(
                    std::next(cfgData.begin(), offset),
                    std::next(cfgData.begin(), offset + chunkSize));

                auto success = false;
                success |= co_await programPage(block, page, chunk);
                co_await sdbusplus::async::sleep_for(ctx, readyPollInterval);
                success |= co_await waitUntilReady(readyTimeout);
                if (!success)
                {
                    lg2::error("Failed to program block {BLOCK} page {PAGE}",
                               "BLOCK", block, "PAGE", page);
                    co_return false;
                }
                bytesWritten += chunkSize;
            }
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::readPage(
    uint8_t block, uint8_t page, std::vector<uint8_t>& data)
{
    if (data.empty())
    {
        lg2::error("Error: data vector is empty.");
        co_return false;
    }
    std::vector<uint8_t> request = {};
    std::vector<uint8_t> response = {};
    request.reserve(4);
    request.push_back(static_cast<uint8_t>(xo5Cmd::pageRead));
    request.push_back(block);
    request.push_back(page);
    request.push_back(0x0);

    if (!i2cInterface.sendReceive(request, response))
    {
        co_return false;
    }
    lg2::debug("Read page {BLOCK} {PAGE} succeeded", "BLOCK", block, "PAGE",
               page);
    request.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (!(co_await waitUntilReady(readyTimeout)))
    {
        co_return false;
    }

    if (!i2cInterface.sendReceive(request, data))
    {
        co_return false;
    }

    co_return data[0] == static_cast<uint8_t>(xo5Status::ready);
}

sdbusplus::async::task<bool> LatticeXO5CPLD::verifyCfg()
{
    const auto& cfgData = fwInfo.cfgData;
    if (isTSeries)
    {
        AUTO_LOCK_I2C;
        std::vector<uint8_t> emptyResponse = {};
        if (!(co_await sendReceive(
                {static_cast<uint8_t>(xo5Cmd::calcHash), 0x0, 0x0, 0x0},
                emptyResponse)))
        {
            lg2::error("Failed to calculate hash command");
            co_return false;
        }
        std::vector<uint8_t> rxDigest = std::vector<uint8_t>(48, 0);
        if (!(co_await sendReceive(
                {static_cast<uint8_t>(xo5Cmd::readHash), 0x0, 0x0, 0x0},
                rxDigest)))
        {
            lg2::error("Failed to read back hash digest");
            co_return false;
        }
        auto digestOpt = calculateSha2_384Openssl(cfgData);
        if (!digestOpt.has_value())
        {
            lg2::error("Failed to compute local SHA2-384 digest");
            co_return false;
        }
        const std::vector<uint8_t>& digest = digestOpt.value();
        if ((digest.size() != rxDigest.size()) ||
            !std::equal(rxDigest.begin(), rxDigest.end(), digest.begin()))
        {
            lg2::error("Hash digest mismatch after programming");
            co_return false;
        }
        lg2::debug("Hash digest verified successfully after programming");
    }
    else
    {
        using diff_t = std::vector<uint8_t>::difference_type;
        const auto totalBytes = cfgData.size();
        uint8_t cfgIndex = getCfgIdx(target);
        uint8_t startBlock;
        if (!getStartBlock(cfgIndex, startBlock))
        {
            lg2::error("Error: invalid cfg index.");
            co_return false;
        }
        const auto endBlock = startBlock + xo5Cfg::blocksPerCfg;
        uint8_t readBuffer[1 + xo5Cfg::pageSize];
        size_t bytesVerified = 0;

        for (size_t block = startBlock; block < endBlock; ++block)
        {
            for (size_t page = 0; page < xo5Cfg::pagesPerBlock; ++page)
            {
                if (bytesVerified >= totalBytes)
                {
                    co_return true;
                }

                auto offset = static_cast<diff_t>(bytesVerified);
                auto remaining =
                    static_cast<diff_t>(totalBytes - bytesVerified);
                const auto chunkSize =
                    std::min(static_cast<diff_t>(xo5Cfg::pageSize), remaining);

                std::vector<uint8_t> expected(
                    std::next(cfgData.begin(), offset),
                    std::next(cfgData.begin(), offset + chunkSize));

                std::vector<uint8_t> chunk;
                {
                    std::vector<uint8_t> readVec(readBuffer,
                                                 readBuffer + 1 + chunkSize);

                    if (co_await readPage(block, page, readVec))
                    {
                        chunk.assign(readVec.begin() + 1, readVec.end());
                    }
                    else
                    {
                        chunk.clear();
                    }
                }

                if (chunk.empty())
                {
                    lg2::error("Failed to read Block {BLOCK} Page {PAGE}",
                               "BLOCK", block, "PAGE", page);
                    co_return false;
                }
                if (!std::equal(chunk.begin(), chunk.end(), expected.begin()))
                {
                    lg2::error("VERIFY FAILED: Block {BLOCK} Page {PAGE}",
                               "BLOCK", block, "PAGE", page);
                    co_return false;
                }

                bytesVerified += chunkSize;
            }
        }
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::readUserCode(uint32_t& userCode)
{
    if (isTSeries)
    {
        if (!(co_await readSoftIPId()))
        {
            lg2::error("Error: read soft IP ID failed.");
            co_return false;
        }
        AUTO_LOCK_I2C;
        uint8_t cfgIndex = getCfgIdx(target);
        std::vector<uint8_t> response = std::vector<uint8_t>(4, 0);
        if ((softIpVersion & 0xF0) == 0x10)
        {
            std::vector<std::vector<uint8_t>> pre_cmds = {
                {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
                {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex,
                 0x0}};
            std::vector<uint8_t> emptyResponse = {};
            for (const auto& cmd : pre_cmds)
            {
                if (!(co_await sendReceive(cmd, emptyResponse)))
                {
                    lg2::error("Failed to send command");
                    co_return false;
                }
            }
            if (!(co_await sendReceive(
                    {static_cast<uint8_t>(xo5Cmd::readUsercode), 0x0, 0x0, 0x0},
                    response)))
            {
                lg2::error("Failed to read user code");
                co_return false;
            }
            if (!(co_await waitUntilReady(readyTimeout)))
            {
                lg2::error("Failed to wait until ready");
                co_return false;
            }
            response = std::vector<uint8_t>(4, 0);
            if (!(co_await sendReceive(
                    {static_cast<uint8_t>(xo5Cmd::readUsercode), 0x0, 0x0, 0x0},
                    response)))
            {
                lg2::error("Failed to read user code");
                co_return false;
            }
            if (!(co_await sendReceive(
                    {static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0},
                    emptyResponse)))
            {
                lg2::error("Failed to disable command");
                co_return false;
            }
        }
        else
        {
            if (!(co_await sendReceive(
                    {static_cast<uint8_t>(xo5Cmd::readUsercode), 0x0, cfgIndex,
                     0x0},
                    response)))
            {
                lg2::error("Failed to read user code");
                co_return false;
            }
        }
        userCode = ((softIpVersion & 0xF0) == 0x20)
                       ? (response[0] << 24) | (response[1] << 16) |
                             (response[2] << 8) | (response[3])
                       : (response[3] << 24) | (response[2] << 16) |
                             (response[1] << 8) | (response[0]);
    }
    else
    {
        constexpr size_t resSize = 5;
        std::vector<uint8_t> request = {commandReadFwVersion, 0x0, 0x0, 0x0};
        std::vector<uint8_t> response(resSize, 0);

        if (!i2cInterface.sendReceive(request, response))
        {
            lg2::error("Failed to send read user code request.");
            co_return false;
        }

        userCode |= response[4] << 24;
        userCode |= response[3] << 16;
        userCode |= response[2] << 8;
        userCode |= response[1];
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::readSoftIPId()
{
    std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::readDeviceID),
                                    0x0, 0x0, 0x0};
    std::vector<uint8_t> deviceID(4, 0);
    if (!(co_await sendReceive(request, deviceID)))
    {
        lg2::error("Failed to send read device ID request.");
        co_return false;
    }
    request = {static_cast<uint8_t>(xo5Cmd::readSoftIpId), 0x0, 0x0, 0x0};
    std::vector<uint8_t> softIPID(5, 0);
    if (!(co_await sendReceive(request, softIPID)))
    {
        lg2::error("Failed to send read soft IP ID request.");
        co_return false;
    }
    uint32_t ipId = (softIPID[0] << 24) | (softIPID[1] << 16) |
                    (softIPID[2] << 8) | (softIPID[3]);
    uint32_t deviceId = (deviceID[0] << 24) | (deviceID[1] << 16) |
                        (deviceID[2] << 8) | (deviceID[3]);
    if (ipId != (deviceId + 1))
    {
        lg2::error("Soft IP ID does not match device");
        co_return false;
    }

    softIpVersion = softIPID[4];

    lg2::info("Soft IP Version: {SOFT_IP_ID}", "SOFT_IP_ID", lg2::hex,
              softIpVersion);
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::prepareUpdate(const uint8_t* image,
                                                           size_t imageSize)
{
    if (target.empty())
    {
        target = "CFG0";
    }
    else if (target != "CFG0" && target != "CFG1")
    {
        lg2::error("Error: unknown target.");
        co_return false;
    }

    if (!jedFileParser(image, imageSize))
    {
        lg2::error("JED file parsing failed");
        co_return false;
    }
    lg2::debug("JED file parsing success");

    if (isTSeries)
    {
        if (!verifyChecksum())
        {
            lg2::error("Checksum verification failed");
            co_return false;
        }

        if (!(co_await readSoftIPId()))
        {
            lg2::error("Error: read soft IP ID failed.");
            co_return false;
        }

        if (!(co_await toggleCrc16(true)))
        {
            lg2::error("Error: toggle crc16 failed.");
            co_return false;
        }
        lg2::debug("Command CRC16 enabled.");
    }

    if (!(co_await waitUntilReady(readyTimeout)))
    {
        lg2::error("Error: Device not ready.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::doErase()
{
    lg2::debug("Erasing {TARGET}...", "TARGET", target);
    if (isTSeries && target == "CFG0")
    {
        if (!(co_await backupHeader()))
        {
            lg2::error("Backup header failed for {TARGET}.", "TARGET", target);
            co_return false;
        }
    }
    if (!(co_await eraseCfg()))
    {
        lg2::error("Erase cfg data failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::doUpdate()
{
    lg2::debug("Programming {TARGET}...", "TARGET", target);
    if (!(co_await programCfg()))
    {
        lg2::error("Program cfg data failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::finishUpdate()
{
    lg2::debug("Verifying {TARGET}...", "TARGET", target);
    if (!(co_await verifyCfg()))
    {
        lg2::error("Verify cfg data failed.");
        co_return false;
    }

    if (isTSeries)
    {
        if (target == "CFG0")
        {
            if (!(co_await restoreHeader()))
            {
                lg2::error("Restore header failed.");
                co_return false;
            }
        }

        uint32_t userCode = 0;
        if (!(co_await readUserCode(userCode)))
        {
            lg2::error("Read usercode failed.");
            co_return false;
        }
        lg2::info("CPLD {TARGET} USERCODE: {USERCODE}", "TARGET", target,
                  "USERCODE", lg2::hex, userCode);
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::readCfg(
    uint8_t targetCfg, std::vector<uint8_t>& dataOut, uint32_t bytesToRead)
{
    uint32_t paddedBytesToRead = bytesToRead;
    if (paddedBytesToRead % xo5Cfg::incrDataSize != 0)
    {
        paddedBytesToRead +=
            xo5Cfg::incrDataSize - (paddedBytesToRead % xo5Cfg::incrDataSize);
    }
    dataOut.clear();
    dataOut.reserve(paddedBytesToRead);

    std::vector<uint8_t> emptyResponse = {};
    std::vector<uint8_t> response;
    std::vector<std::vector<uint8_t>> preReadCmds = {
        {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
        {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, targetCfg, 0x0}};

    for (const auto& cmd : preReadCmds)
    {
        if (!(co_await sendReceive(cmd, emptyResponse)))
        {
            lg2::error("Failed to send command opcode: {OPCODE}", "OPCODE",
                       lg2::hex, cmd[0]);
            co_return false;
        }
    }
    lg2::debug("Pre-readback completed successfully");

    for (uint32_t offset = 0; offset < paddedBytesToRead;
         offset += xo5Cfg::incrDataSize)
    {
        response = std::vector<uint8_t>(xo5Cfg::incrDataSize, 0);
        bool success = co_await sendReceive(
            {static_cast<uint8_t>(xo5Cmd::readIncr), 0x0, 0x0, 0x0}, response);
        if (!success)
        {
            lg2::error("Failed to read incr");
            co_return false;
        }
        dataOut.insert(dataOut.end(), response.begin(), response.end());
    }
    lg2::debug("Readback data completed successfully");

    if (!(co_await sendReceive(
            {static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x00, 0x0},
            emptyResponse)))
    {
        lg2::error("Failed to send command opcode: {OPCODE}", "OPCODE",
                   lg2::hex, static_cast<uint8_t>(xo5Cmd::disable));
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::backupHeader()
{
    AUTO_LOCK_I2C
    std::vector<uint8_t> headerData;
    if (!(co_await readCfg(xo5Cfg::headerIdx, headerData, 0x1000)))
    {
        lg2::error("Failed to read HEADER data");
        co_return false;
    }
    if (std::all_of(headerData.begin(), headerData.end(),
                    [](uint8_t b) { return b == 0xFF; }))
    {
        lg2::debug("HEADER data is empty, skipping backup.");
        co_return true;
    }
    lg2::debug("Erasing reserved UFM before writing header backup ...");
    if (!(co_await eraseCfg(xo5Cfg::ufm8Idx)))
    {
        lg2::error("Failed to erase reserved UFM before writing header backup");
        co_return false;
    }
    lg2::debug("Writing HEADER backup to reserved UFM ...");
    if (!(co_await programCfg(xo5Cfg::ufm8Idx, &headerData)))
    {
        lg2::error("Failed to write HEADER backup to reserved UFM");
        co_return false;
    }
    lg2::debug("Erasing HEADER area ...");
    if (!(co_await eraseCfg(xo5Cfg::headerIdx)))
    {
        lg2::error("Failed to erase HEADER area");
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::restoreHeader()
{
    AUTO_LOCK_I2C;
    std::vector<uint8_t> headerData;
    if (!(co_await readCfg(xo5Cfg::ufm8Idx, headerData, 0x1000)))
    {
        lg2::error("Failed to read HEADER backup data from reserved UFM");
        co_return false;
    }
    if (std::all_of(headerData.begin(), headerData.end(),
                    [](uint8_t b) { return b == 0xFF; }))
    {
        lg2::debug("Backup header data is empty, skipping restore.");
        co_return true;
    }
    lg2::debug("Erasing HEADER area ...");
    if (!(co_await eraseCfg(xo5Cfg::headerIdx)))
    {
        lg2::error("Failed to erase HEADER area");
        co_return false;
    }
    lg2::debug("Writing HEADER data to HEADER area ...");
    if (!(co_await programCfg(xo5Cfg::headerIdx, &headerData)))
    {
        lg2::error("Failed to write HEADER data to HEADER area");
        co_return false;
    }
    co_return true;
}
} // namespace phosphor::software::cpld
