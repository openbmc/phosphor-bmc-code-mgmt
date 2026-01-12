#include "lattice_xo5_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{

constexpr std::chrono::milliseconds ReadyPollInterval(10);
constexpr std::chrono::milliseconds ReadyTimeout(1000);

enum class xo5Cmd : uint8_t
{
    sectorErase = 0xd8,
    pageProgram = 0x02,
    pageRead = 0x0b,
    readUsercode = 0xc0
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

sdbusplus::async::task<bool> LatticeXO5CPLD::waitUntilReady(
    std::chrono::milliseconds timeout)
{
    const auto endTime = std::chrono::steady_clock::now() + timeout;

    auto readDummy = [this]() -> sdbusplus::async::task<bool> {
        std::vector<uint8_t> request = {};
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
        co_await sdbusplus::async::sleep_for(ctx, ReadyPollInterval);
    }

    lg2::error("Timeout waiting for device ready");
    co_return false;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::eraseCfg()
{
    auto cfgIndex = (target == "CFG0") ? 0 : 1;
    uint8_t startBlock;
    if (!getStartBlock(cfgIndex, startBlock))
    {
        lg2::error("Error: invalid cfg index.");
        co_return false;
    }
    const auto endBlock = startBlock + xo5Cfg::blocksPerCfg;

    auto eraseBlock = [this](uint8_t block) -> sdbusplus::async::task<bool> {
        std::vector<uint8_t> request;
        std::vector<uint8_t> response = {};
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
        if (!(co_await waitUntilReady(ReadyTimeout)))
        {
            lg2::error("Failed to wait until ready");
            co_return false;
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

sdbusplus::async::task<bool> LatticeXO5CPLD::programCfg()
{
    using diff_t = std::vector<uint8_t>::difference_type;

    auto cfgIndex = (target == "CFG0") ? 0 : 1;
    uint8_t startBlock;
    if (!getStartBlock(cfgIndex, startBlock))
    {
        lg2::error("Error: invalid cfg index.");
        co_return false;
    }
    const auto endBlock = startBlock + xo5Cfg::blocksPerCfg;
    const auto& cfgData = fwInfo.cfgData;
    const auto totalBytes = cfgData.size();
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
            co_await sdbusplus::async::sleep_for(ctx, ReadyPollInterval);
            success |= co_await waitUntilReady(ReadyTimeout);
            if (!success)
            {
                lg2::error("Failed to program block {BLOCK} page {PAGE}",
                           "BLOCK", block, "PAGE", page);
                co_return false;
            }
            bytesWritten += chunkSize;
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

    if (!(co_await waitUntilReady(ReadyTimeout)))
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
    using diff_t = std::vector<uint8_t>::difference_type;

    auto cfgIndex = (target == "CFG0") ? 0 : 1;
    uint8_t startBlock;
    if (!getStartBlock(cfgIndex, startBlock))
    {
        lg2::error("Error: invalid cfg index.");
        co_return false;
    }
    const auto endBlock = startBlock + xo5Cfg::blocksPerCfg;
    const auto& cfgData = fwInfo.cfgData;
    const auto totalBytes = cfgData.size();
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
            auto remaining = static_cast<diff_t>(totalBytes - bytesVerified);
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
                lg2::error("Failed to read Block {BLOCK} Page {PAGE}", "BLOCK",
                           block, "PAGE", page);
                co_return false;
            }
            if (!std::equal(chunk.begin(), chunk.end(), expected.begin()))
            {
                lg2::error("VERIFY FAILED: Block {BLOCK} Page {PAGE}", "BLOCK",
                           block, "PAGE", page);
                co_return false;
            }

            bytesVerified += chunkSize;
        }
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::readUserCode(uint32_t& userCode)
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

    if (!(co_await waitUntilReady(ReadyTimeout)))
    {
        lg2::error("Error: Device not ready.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD::doErase()
{
    lg2::debug("Erasing {TARGET}...", "TARGET", target);
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
    co_return true;
}

} // namespace phosphor::software::cpld
