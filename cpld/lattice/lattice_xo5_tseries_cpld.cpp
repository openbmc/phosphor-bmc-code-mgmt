#include "lattice_xo5_tseries_cpld.hpp"

#include <openssl/sha.h>

#include <phosphor-logging/lg2.hpp>

namespace phosphor::software::cpld
{
namespace
{
constexpr std::chrono::milliseconds tSeriesReadyPollInterval{1};
constexpr uint16_t crc16MsbMask = 0x8000;
constexpr uint16_t crc16Polynomial = 0x1021;
constexpr uint8_t targetSlotCfg1 = 2;
constexpr uint8_t targetSlotCfg0 = 1;
constexpr uint8_t softIpMask = 0xF0;
constexpr uint8_t softIpV1 = 0x10;
constexpr uint8_t softIpV2 = 0x20;
} // namespace
LatticeXO5TSeriesCPLD::LatticeXO5TSeriesCPLD(
    sdbusplus::async::context& ctx, uint16_t bus, uint8_t address,
    const std::string& chip, const std::string& target, bool debugMode) :
    LatticeXO5BaseCPLD(ctx, bus, address, chip, target,
                       tSeriesReadyPollInterval, debugMode)
{}

std::optional<std::vector<uint8_t>>
    LatticeXO5TSeriesCPLD::calculateSha2_384Openssl(
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

uint16_t LatticeXO5TSeriesCPLD::crc16Ccitt(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j)
        {
            if (crc & crc16MsbMask)
            {
                crc = (crc << 1) ^ crc16Polynomial;
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t LatticeXO5TSeriesCPLD::appendCrc16(std::vector<uint8_t>& data)
{
    uint16_t crc = crc16Ccitt(&data[0], data.size());
    data.push_back(static_cast<uint8_t>(crc & 0xFF));
    data.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    return crc;
}

uint8_t LatticeXO5TSeriesCPLD::getCfgIdx(std::string_view target) const
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

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::lockI2C()
{
    if ((softIpVersion & softIpMask) != softIpV2)
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

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::toggleCrc16(bool enable)
{
    if (!(co_await lockI2C()))
    {
        co_return false;
    }
    std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::controlCmdCrc),
                                    static_cast<uint8_t>(enable ? 0x01 : 0x00),
                                    0x00, 0x00};
    std::vector<uint8_t> response = {};

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to toggle CRC16.");
        co_return false;
    }
    crc16Enabled = enable;
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::readCrcStatus(bool& crc_ok)
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

bool LatticeXO5TSeriesCPLD::isCrcRequired(uint8_t opcode) const
{
    if (opcode == static_cast<uint8_t>(xo5Cmd::programIncr) ||
        opcode == static_cast<uint8_t>(xo5Cmd::readIncr) ||
        opcode == static_cast<uint8_t>(xo5Cmd::readSoftIpId))
    {
        return true;
    }

    bool isV20 = (softIpVersion & softIpMask) == softIpV2;
    if (opcode == static_cast<uint8_t>(xo5Cmd::controlCmdCrc) ||
        opcode == static_cast<uint8_t>(xo5Cmd::checkBusyStatus) ||
        opcode == static_cast<uint8_t>(xo5Cmd::readStatusReg) ||
        opcode == static_cast<uint8_t>(xo5Cmd::refresh) ||
        (isV20 && (opcode == static_cast<uint8_t>(xo5Cmd::readDeviceID) ||
                   opcode == static_cast<uint8_t>(xo5Cmd::readUsercode) ||
                   opcode == static_cast<uint8_t>(xo5Cmd::lock))))
    {
        return false;
    }

    return crc16Enabled;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::isCrcSuccessful(
    uint8_t opcode, const std::vector<uint8_t>& responseCrc)
{
    if (opcode == static_cast<uint8_t>(xo5Cmd::programIncr))
    {
        co_return !responseCrc.empty() && (responseCrc[0] & 0x01) == 0x01;
    }

    bool crcOk = false;
    if (!(co_await readCrcStatus(crcOk)))
    {
        lg2::error("Failed to read CRC status for opcode {OPCODE}", "OPCODE",
                   lg2::hex, opcode);
        co_return false;
    }
    co_return crcOk;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::sendReceive(
    const std::vector<uint8_t>& request, std::vector<uint8_t>& response)
{
    if (request.empty())
    {
        lg2::error("Error: request vector is empty.");
        co_return false;
    }

    uint8_t opcode = request.at(0);

    if (!isCrcRequired(opcode))
    {
        co_return i2cInterface.sendReceive(request, response);
    }

    std::vector<uint8_t> request_crc = request;
    appendCrc16(request_crc);
    std::vector<uint8_t> responseCrc = response;
    if (!responseCrc.empty() &&
        (opcode != static_cast<uint8_t>(xo5Cmd::programIncr)))
    {
        responseCrc.resize(response.size() + 2, 0x00);
    }

    std::size_t j = 0;
    for (; j < xo5Cfg::retryMax; ++j)
    {
        if (!i2cInterface.sendReceive(request_crc, responseCrc))
        {
            lg2::error("Failed to sendReceive with CRC16.");
            co_return false;
        }
        if (co_await isCrcSuccessful(opcode, responseCrc))
        {
            if (!response.empty() &&
                (responseCrc.size() == response.size() + 2))
            {
                std::copy(responseCrc.begin(), responseCrc.end() - 2,
                          response.begin());
            }
            co_return true;
        }
        lg2::warning("CRC16 check failed, retrying... Attempt {ATTEMPT}",
                     "ATTEMPT", j + 1);
    }

    if (j == xo5Cfg::retryMax)
    {
        lg2::error(
            "Failed to sendReceive with CRC16 after {MAX_RETRIES} attempts.",
            "MAX_RETRIES", xo5Cfg::retryMax);
    }
    co_return false;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::readSoftIPId()
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
    uint32_t ipId = extractUint32(softIPID, 0, true);
    uint32_t deviceId = extractUint32(deviceID, 0, true);
    if (ipId != (deviceId + 1))
    {
        lg2::error("Soft IP ID does not match device");
        co_return false;
    }

    softIpVersion = softIPID[4];

    lg2::debug("Soft IP Version: {SOFT_IP_ID}", "SOFT_IP_ID", lg2::hex,
               softIpVersion);
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::prepareUpdate(
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

    if (!(co_await waitUntilReady(readyTimeout)))
    {
        lg2::error("Error: Device not ready.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::doErase()
{
    lg2::debug("Erasing {TARGET}...", "TARGET", target);
    if (target == "CFG0")
    {
        if (!(co_await backupHeader()))
        {
            lg2::error("Backup header failed.");
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

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::finishUpdate()
{
    lg2::debug("Verifying {TARGET}...", "TARGET", target);
    if (!(co_await verifyCfg()))
    {
        co_return false;
    }

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
    lg2::debug("CPLD {TARGET} USERCODE: {USERCODE}", "TARGET", target,
               "USERCODE", lg2::hex, userCode);
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::readUserCode(
    uint32_t& userCode)
{
    if (!(co_await readSoftIPId()))
    {
        lg2::error("Error: read soft IP ID failed.");
        co_return false;
    }
    if (!(co_await lockI2C()))
    {
        co_return false;
    }
    uint8_t cfgIndex = getCfgIdx(target);
    std::vector<uint8_t> response = std::vector<uint8_t>(4, 0);
    if ((softIpVersion & softIpMask) == softIpV1)
    {
        std::vector<std::vector<uint8_t>> pre_cmds = {
            {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
            {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex, 0x0}};
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
        if (!(co_await sendReceive({static_cast<uint8_t>(xo5Cmd::readUsercode),
                                    0x0, cfgIndex, 0x0},
                                   response)))
        {
            lg2::error("Failed to read user code");
            co_return false;
        }
    }

    bool isBigEndian = ((softIpVersion & softIpMask) == softIpV2);
    userCode = extractUint32(response, 0, isBigEndian);
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::eraseCfg(
    std::optional<uint8_t> setIdx)
{
    uint8_t cfgIndex = setIdx.has_value() ? setIdx.value() : getCfgIdx(target);
    std::vector<uint8_t> response = {};
    if (!(co_await lockI2C()))
    {
        co_return false;
    }
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
            {static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0}, response)))
    {
        lg2::error("Failed to disable command");
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::programCfg(
    std::optional<uint8_t> setIdx, const std::vector<uint8_t>* customData)
{
    uint8_t cfgIndex = setIdx.has_value() ? setIdx.value() : getCfgIdx(target);
    const std::vector<uint8_t>& cfgData =
        (customData != nullptr) ? *customData : fwInfo.cfgData;

    if (!(co_await lockI2C()))
    {
        co_return false;
    }

    std::vector<uint8_t> emptyResponse = {};
    std::vector<uint8_t> response;
    std::vector<std::vector<uint8_t>> preCmds = {
        {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
        {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex, 0x0},
        {static_cast<uint8_t>(xo5Cmd::calcHash), 0x0, 0x0, 0x0}};

    for (const auto& cmd : preCmds)
    {
        if (!(co_await sendReceive(cmd, emptyResponse)))
        {
            co_return false;
        }
    }

    std::vector<uint8_t> paddedData = cfgData;
    const size_t chunkSize = xo5Cfg::incrDataSize;
    size_t totalBytes = paddedData.size();
    if (totalBytes % chunkSize != 0)
    {
        paddedData.insert(paddedData.end(),
                          chunkSize - (totalBytes % chunkSize), 0xFF);
        totalBytes = paddedData.size();
    }

    unsigned int i = 0;
    for (size_t offset = 0; offset < totalBytes; offset += chunkSize)
    {
        std::vector<uint8_t> chunk = {static_cast<uint8_t>(xo5Cmd::programIncr),
                                      0x0, 0x0, 0x0};
        auto startIdx = static_cast<std::ptrdiff_t>(offset);
        auto endIdx = static_cast<std::ptrdiff_t>(offset + chunkSize);

        chunk.insert(chunk.end(), std::next(paddedData.begin(), startIdx),
                     std::next(paddedData.begin(), endIdx));
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

    std::vector<std::vector<uint8_t>> postCmds = {
        {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x00, 0x0},
        {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex, 0x0},
        {static_cast<uint8_t>(xo5Cmd::programDone), 0x0, 0x0, 0x0},
        {static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0}};

    for (const auto& cmd : postCmds)
    {
        if (!(co_await sendReceive(cmd, emptyResponse)))
        {
            co_return false;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::verifyCfg()
{
    if (!(co_await lockI2C()))
    {
        co_return false;
    }

    std::vector<uint8_t> response;
    if (!(co_await sendReceive(
            {static_cast<uint8_t>(xo5Cmd::calcHash), 0, 0, 0}, response)))
    {
        lg2::error("Failed to calculate hash command");
        co_return false;
    }

    std::vector<uint8_t> rxDigest(48, 0);
    if (!(co_await sendReceive(
            {static_cast<uint8_t>(xo5Cmd::readHash), 0, 0, 0}, rxDigest)))
    {
        lg2::error("Failed to read back hash digest");
        co_return false;
    }

    auto digestOpt = calculateSha2_384Openssl(fwInfo.cfgData);
    if (!digestOpt.has_value())
    {
        lg2::error("Failed to compute local SHA2-384 digest");
        co_return false;
    }

    const auto& localDigest = digestOpt.value();
    if (rxDigest != localDigest)
    {
        lg2::error("Hash digest mismatch after programming");
        co_return false;
    }

    lg2::debug("Hash digest verified successfully");
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::readCfg(
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

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::backupHeader()
{
    if (!(co_await lockI2C()))
    {
        co_return false;
    }
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

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::restoreHeader()
{
    if (!(co_await lockI2C()))
    {
        co_return false;
    }
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

sdbusplus::async::task<bool> LatticeXO5TSeriesCPLD::checkDeviceReady()
{
    if (!(co_await lockI2C()))
    {
        co_return false;
    }

    std::vector<uint8_t> request = {
        static_cast<uint8_t>(xo5Cmd::checkBusyStatus), 0x00, 0x00, 0x00};
    std::vector<uint8_t> response = {0xff};

    if (!i2cInterface.sendReceive(request, response))
    {
        co_return false;
    }

    co_return response.at(0) == static_cast<uint8_t>(xo5Status::ready);
}

} // namespace phosphor::software::cpld
