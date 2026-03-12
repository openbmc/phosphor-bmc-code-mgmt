#include "lattice_xo5_standard_cpld.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{
namespace
{
constexpr std::chrono::milliseconds readyPollInterval{10};
constexpr std::chrono::milliseconds pageReadDelay{1};
constexpr uint8_t targetSlotCfg1 = 1;
constexpr uint8_t targetSlotCfg0 = 0;
} // namespace
LatticeXO5StandardCPLD::LatticeXO5StandardCPLD(
    sdbusplus::async::context& ctx, uint16_t bus, uint8_t address,
    const std::string& chip, const std::string& target, bool debugMode) :
    LatticeXO5BaseCPLD(ctx, bus, address, chip, target, readyPollInterval,
                       debugMode)
{}

uint8_t LatticeXO5StandardCPLD::getCfgIdx(std::string_view target) const
{
    if (target.empty())
    {
        return 0;
    }
    std::string lowerTarget(target);
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
                   ::tolower);
    return (lowerTarget == "cfg1") ? targetSlotCfg1 : targetSlotCfg0;
}

bool getStartBlock(uint8_t cfg, uint8_t& startBlock)
{
    static constexpr std::array<uint8_t, 3> cfgStartBlocks = {0x01, 0x10, 0x1F};

    if (cfg >= cfgStartBlocks.size())
    {
        return false;
    }

    startBlock = cfgStartBlocks[cfg];
    return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::eraseCfg(
    [[maybe_unused]] std::optional<uint8_t> setIdx)
{
    auto cfgIndex = getCfgIdx(target);
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
        if (!(co_await waitUntilReady(readyTimeout)))
        {
            lg2::error("Failed to wait until ready");
            co_return false;
        }
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::programPage(
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

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::programCfg(
    [[maybe_unused]] std::optional<uint8_t> setIdx,
    [[maybe_unused]] const std::vector<uint8_t>* customData)
{
    using diff_t = std::vector<uint8_t>::difference_type;

    auto cfgIndex = getCfgIdx(target);
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

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::readPage(
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

    co_await sdbusplus::async::sleep_for(ctx, pageReadDelay);

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

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::verifyCfg()
{
    using diff_t = std::vector<uint8_t>::difference_type;

    auto cfgIndex = getCfgIdx(target);
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

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::readUserCode(
    uint32_t& userCode)
{
    constexpr size_t resSize = 5;
    std::vector<uint8_t> request = {commandReadFwVersion, 0x0, 0x0, 0x0};
    std::vector<uint8_t> response(resSize, 0);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send read user code request.");
        co_return false;
    }

    userCode = extractUint32(response, 1, false);

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::programDone()
{
    std::vector<uint8_t> request = {};
    std::vector<uint8_t> response = {};
    request.push_back(static_cast<uint8_t>(xo5Cmd::programDone));
    request.push_back(0x0);
    request.push_back(0x0);
    request.push_back(0x0);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to send program done request.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::prepareUpdate(
    const uint8_t* image, size_t imageSize)
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

    if (!(co_await waitUntilReady(readyTimeout)))
    {
        lg2::error("Error: Device not ready.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::doErase()
{
    lg2::debug("Erasing {TARGET}...", "TARGET", target);
    if (!(co_await eraseCfg()))
    {
        lg2::error("Erase cfg data failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::finishUpdate()
{
    lg2::debug("Verifying {TARGET}...", "TARGET", target);
    if (!(co_await verifyCfg()))
    {
        lg2::error("Verify cfg data failed.");
        co_return false;
    }

    if (!(co_await programDone()))
    {
        lg2::error("Send program done request failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5StandardCPLD::checkDeviceReady()
{
    std::vector<uint8_t> request = {};
    std::vector<uint8_t> response = {0xff};

    if (!i2cInterface.sendReceive(request, response))
    {
        co_return false;
    }

    co_return response.at(0) == static_cast<uint8_t>(xo5Status::ready);
}
} // namespace phosphor::software::cpld
