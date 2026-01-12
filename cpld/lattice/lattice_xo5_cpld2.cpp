#include "lattice_xo5_cpld2.hpp"

#include <phosphor-logging/lg2.hpp>
#include <openssl/sha.h>

namespace phosphor::software::cpld
{

constexpr std::chrono::milliseconds ReadyPollInterval(10);
constexpr std::chrono::milliseconds ReadyTimeout(1000);
constexpr std::chrono::milliseconds EraseTimeout(20000);

enum class xo5Cmd : uint8_t
{
    noop = 0xff,
    noop0 = 0x00,
    readDeviceID = 0xe0,
    readSoftIPID= 0xe6,
    readUsercode = 0xc0,
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
    readhash = 0xe5,    
    controlCmdCrc = 0xfd
};

enum class xo5Status : uint8_t
{
    ready = 0x00,
    notReady = 0xff
};

struct xo5Cfg
{
    static constexpr size_t incrDataSize = 128;
    static constexpr size_t retryMax = 3;
};

// crc16-ccitt calculation
static uint16_t crc16_ccitt(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

static uint16_t append_crc16(std::vector<uint8_t>& data)
{
    // CRC[0] is low 8 bits  crc[1] is high 8 bits
    uint16_t crc = crc16_ccitt(&data[0], data.size());
    data.push_back(static_cast<uint8_t>(crc & 0xFF));
    data.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    return crc;
}

static std::optional<std::vector<uint8_t>> calculate_sha2_384_openssl(const std::vector<uint8_t>& input)
{
    std::vector<uint8_t> digest(SHA384_DIGEST_LENGTH);
    if (SHA384(input.data(), input.size(), digest.data()) == nullptr)
    {
        lg2::error("Failed to compute SHA2-384 digest using OpenSSL SHA384()");
        return std::nullopt;
    }
    return digest; 
}

static std::optional<uint8_t> getCfgIdx(std::string_view target)
{
    // Convert input to uppercase for case-insensitive comparison
    std::string lowerTarget(target);
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
                   ::tolower);

    const std::map<std::string, uint8_t> targetMap = {{"cfg0", 1}, {"cfg1", 2}};

    auto it = targetMap.find(lowerTarget);
    if (it != targetMap.end())
        return it->second;
    return std::nullopt;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::waitUntilReady(
    std::chrono::milliseconds timeout)
{
    const auto endTime = std::chrono::steady_clock::now() + timeout;

    auto readDummy = [this]() -> sdbusplus::async::task<bool> {
        std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::checkBusyStatus), 0x00, 0x00, 0x00};
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

sdbusplus::async::task<bool> LatticeXO5CPLD2::toggle_crc16(bool enable)
{
    std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::controlCmdCrc),
                                    static_cast<uint8_t>(enable ? 0x01 : 0x00),
                                    0x00,
                                    0x00};
    std::vector<uint8_t> response = {};

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to toggle CRC16.");
        co_return false;
    }
    crc16Enabled_ = enable;
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::read_crc_status(bool& crc_ok)
{
    std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::checkBusyStatus), 0x00, 0x00, 0x00};
    std::vector<uint8_t> response(1, 0xFF);

    if (!i2cInterface.sendReceive(request, response))
    {
        lg2::error("Failed to read CRC status.");
        co_return false;
    }

    // crc is bit 1. ok == 0, bad == 1
    crc_ok = (response.at(0) & 0x02) == 0;
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::sendReceive(const std::vector<uint8_t>& request, std::vector<uint8_t>& response)
{
    if (request.empty())
    {
        lg2::error("Error: request vector is empty.");
        co_return false;
    }

    bool crc16Enabled = false;
    uint8_t opcode = request.at(0);
    if (opcode == static_cast<uint8_t>(xo5Cmd::programIncr) ||
        opcode == static_cast<uint8_t>(xo5Cmd::readIncr))
    {
        // programIncr and readIncr commands are always sent with CRC16
        crc16Enabled = true;
    }
    else if ((opcode == static_cast<uint8_t>(xo5Cmd::controlCmdCrc)) ||
             (opcode == static_cast<uint8_t>(xo5Cmd::checkBusyStatus)) ||
             (opcode == static_cast<uint8_t>(xo5Cmd::readStatusReg)) ||
             (opcode == static_cast<uint8_t>(xo5Cmd::refresh)))
    {
        // controlCmdCrc, checkBusyStatus, readStatusReg, refresh commands are always sent without CRC16
        crc16Enabled = false;
    }
    else
    {
        // for other commands, use the current setting
        crc16Enabled = crc16Enabled_;
    }

    // send without CRC16
    if (!crc16Enabled)
    {
        co_return i2cInterface.sendReceive(request, response);
    }
    
    // send with CRC16
    std::vector<uint8_t> request_crc = request;
    append_crc16(request_crc);
    std::vector<uint8_t> response_crc = response;
    if (!response_crc.empty() && (opcode != static_cast<uint8_t>(xo5Cmd::programIncr))) {
        // placeholder for CRC16 bytes
        response_crc.push_back(0x00);
        response_crc.push_back(0x00);
    }

    std::size_t j = 0;
    for (; j < xo5Cfg::retryMax; ++j) {
        if (!i2cInterface.sendReceive(request_crc, response_crc)) {
            lg2::error("Failed to sendReceive with CRC16.");
            co_return false;
        }
        // check crc 
        bool crc_ok = false;
        if (opcode == static_cast<uint8_t>(xo5Cmd::programIncr)) {
            // crc is bit 0. ok == 1, bad == 0
            crc_ok = (response_crc.size() > 0 && (response_crc[0] & 0x01) == 0x01);
        } else if (! (co_await read_crc_status(crc_ok))) {
            lg2::error("Failed to read CRC status.");
            co_return false;
        }
        if (crc_ok) {
            if (response.size() > 0 && (response_crc.size() == response.size() + 2)) {             
                std::copy(response_crc.begin(), response_crc.end() - 2, response.begin());
            }
            co_return true;
        } else {
            lg2::warning("CRC16 check failed, retrying... Attempt {ATTEMPT}", "ATTEMPT", j+1);
            continue;
        }
    }

    if (j == xo5Cfg::retryMax) {
        lg2::error("Failed to sendReceive with CRC16 after {MAX_RETRIES} attempts.", "MAX_RETRIES", xo5Cfg::retryMax);
    }
    co_return false;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::eraseCfg()
{
    auto cfgIndex = getCfgIdx(target);
    if (!cfgIndex.has_value()) {
        lg2::error("Error: invalid cfg index.");
        co_return false;
    }
    std::vector<uint8_t> emptyResponse = {};

    std::vector<std::vector<uint8_t>> cmds = {
        {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
        {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex.value(), 0x0},
        {static_cast<uint8_t>(xo5Cmd::erase), 0x0, cfgIndex.value(), 0x0}
    };

    for (const auto& cmd : cmds) {
        if (!(co_await sendReceive(cmd, emptyResponse))) {
            lg2::error("Failed to send command");
            co_return false;
        }
    }

    if (!(co_await waitUntilReady(EraseTimeout))) {
        lg2::error("Failed to wait until ready");
        co_return false;
    }

    if (!(co_await sendReceive({static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0}, emptyResponse))) {
        lg2::error("Failed to disable command");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::programCfg()
{
    auto cfgIndex = getCfgIdx(target);
    if (!cfgIndex.has_value()) {
        lg2::error("Error: invalid cfg index.");
        co_return false;
    }

    std::vector<uint8_t> emptyResponse = {};
    std::vector<uint8_t> response;
    // pre-write commands
    std::vector<std::vector<uint8_t>> preWriteCmds = {
        {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
        {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex.value(), 0x0},
        {static_cast<uint8_t>(xo5Cmd::calcHash), 0x0, 0x0, 0x0}
    };
    
    for (const auto& cmd : preWriteCmds) {
        if (!(co_await sendReceive(cmd, emptyResponse))) {
            lg2::error("Failed to send command opcode: {OPCODE}", "OPCODE", lg2::hex, cmd[0]);
            co_return false;
        }
    }
    lg2::debug("Pre-programming completed successfully");

    // write cfg data in chunks
    std::vector<uint8_t> cfgData = fwInfo.cfgData;
    auto totalBytes = cfgData.size();
    const auto chunkSize = xo5Cfg::incrDataSize;
    // pad cfg data to multiple of chunkSize
    if (totalBytes % chunkSize != 0)
    {
        cfgData.insert(cfgData.end(), chunkSize - (totalBytes % chunkSize), 0xFF);
        totalBytes = cfgData.size();
    }

    for (size_t offset = 0; offset < totalBytes; offset += chunkSize) {
        std::vector<uint8_t> chunk = {static_cast<uint8_t>(xo5Cmd::programIncr), 0x0, 0x0, 0x0};
        chunk.insert(chunk.end(), cfgData.begin() + offset, cfgData.begin() + offset + chunkSize);
        append_crc16(chunk);
        response = {0xFF};
        bool success = co_await sendReceive(chunk, response);
        success &= co_await waitUntilReady(ReadyTimeout);
        if (!success) {
            lg2::error("Failed to program incr");
            co_return false;
        }
    }
    lg2::debug("Programming data completed successfully");

    // post-write commands
    std::vector<std::vector<uint8_t>> postWriteCmds = {
        {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x00, 0x0},
        {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex.value(), 0x0},
        {static_cast<uint8_t>(xo5Cmd::programDone), 0x0, 0x0, 0x0},
        {static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0}
    };
    for (const auto& cmd : postWriteCmds) {
        if (!(co_await sendReceive(cmd, emptyResponse))) {
            lg2::error("Failed to send command opcode: {OPCODE:X}", "OPCODE", lg2::hex, cmd[0]);
            co_return false;
        }
    }
    lg2::debug("Post-programming completed successfully");

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::verifyCfg()
{
    std::vector<uint8_t> emptyResponse = {};
    std::vector<uint8_t> cfgData = fwInfo.cfgData;

    // Verify digest of sha-384
    if (!(co_await sendReceive({static_cast<uint8_t>(xo5Cmd::calcHash), 0x0, 0x0, 0x0}, emptyResponse))) {
        lg2::error("Failed to calculate hash command");
        co_return false;
    }
    std::vector<uint8_t> rxDigest = std::vector<uint8_t>(48, 0);
    if (!(co_await sendReceive({static_cast<uint8_t>(xo5Cmd::readhash), 0x0, 0x0, 0x0}, rxDigest))) {
        lg2::error("Failed to read back hash digest");
        co_return false;
    }
    // using OpenSSL to compute sha2-384 compute local digest
    auto digest_opt = calculate_sha2_384_openssl(cfgData);
    if (!digest_opt.has_value()) {
        lg2::error("Failed to compute local SHA2-384 digest");
        co_return false;
    }
    const std::vector<uint8_t>& digest = digest_opt.value();
    if ((digest.size() != rxDigest.size()) || !std::equal(rxDigest.begin(), rxDigest.end(), digest.begin())) {
        lg2::error("Hash digest mismatch after programming");
        co_return false;
    }
    lg2::debug("Hash digest verified successfully after programming");

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::readUserCode(uint32_t& userCode)
{
    auto cfgIndex = getCfgIdx(target);
    if (!cfgIndex.has_value()) {
        lg2::error("Error: invalid cfg index.");
        co_return false;
    }

    std::vector<std::vector<uint8_t>> pre_cmds = {
        {static_cast<uint8_t>(xo5Cmd::enablex), 0x0, 0x0, 0x0},
        {static_cast<uint8_t>(xo5Cmd::initAddress), 0x0, cfgIndex.value(), 0x0}
    };
    std::vector<uint8_t> emptyResponse = {};
    for (const auto& cmd : pre_cmds)
    {
        if (!(co_await sendReceive(cmd, emptyResponse)))
        {
            lg2::error("Failed to send command");
            co_return false;
        }
    }

    // send first readUsercode command to initiate read
    std::vector<uint8_t> response = std::vector<uint8_t>(4, 0);
    if (!(co_await sendReceive({static_cast<uint8_t>(xo5Cmd::readUsercode), 0x0, 0x0, 0x0}, response))) {
        lg2::error("Failed to read user code");
        co_return false;
    }
    if (!(co_await waitUntilReady(ReadyTimeout))) {
        lg2::error("Failed to wait until ready");
        co_return false;
    }
    // send second readUsercode command to get data
    response = std::vector<uint8_t>(4, 0);
    if (!(co_await sendReceive({static_cast<uint8_t>(xo5Cmd::readUsercode), 0x0, 0x0, 0x0}, response))) {
        lg2::error("Failed to read user code");
        co_return false;
    }
    // disable command
    if (!(co_await sendReceive({static_cast<uint8_t>(xo5Cmd::disable), 0x0, 0x0, 0x0}, emptyResponse))) {
        lg2::error("Failed to disable command");
        co_return false;
    }
    userCode = (response[3] << 24) | (response[2] << 16) | (response[1] << 8) | response[0];
    //lg2::debug("User Code: {USER_CODE}", "USER_CODE", lg2::hex, userCode);
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::readSoftIPId()
{
    // read device ID 
    std::vector<uint8_t> request = {static_cast<uint8_t>(xo5Cmd::readDeviceID), 0x0, 0x0, 0x0};
    std::vector<uint8_t> deviceID(4, 0);
    if (!(co_await sendReceive(request, deviceID)))
    {
        lg2::error("Failed to send read device ID request.");
        co_return false;
    }
    // read soft IP ID
    request = {static_cast<uint8_t>(xo5Cmd::readSoftIPID), 0x0, 0x0, 0x0};
    std::vector<uint8_t> softIPID(5, 0);
    if (!(co_await sendReceive(request, softIPID)))
    {
        lg2::error("Failed to send read soft IP ID request.");
        co_return false;
    }
    // check soft IP ID[0:3] == device ID[0:3] + 1
    uint32_t ipId = (softIPID[0] << 24) | (softIPID[1] << 16) | (softIPID[2] << 8) | softIPID[3];
    uint32_t deviceId = (deviceID[0] << 24) | (deviceID[1] << 16) | (deviceID[2] << 8) | deviceID[3];
    if (ipId != (deviceId + 1)) {
        lg2::error("Soft IP ID does not match device");
        co_return false;
    }
    
    lg2::debug("Soft IP Version: {SOFT_IP_ID}", "SOFT_IP_ID", lg2::hex, softIPID[4]);
    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::prepareUpdate(const uint8_t* image,
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

    if (!verifyChecksum()) {
        lg2::error("Checksum verification failed");
        co_return false;
    }
    lg2::debug("JED file parsing success");

    // turn on/off crc16 before any operations
    if (!(co_await toggle_crc16(true))) {
        lg2::error("Error: toggle crc16 failed.");
        co_return false;
    }
    lg2::debug("Command CRC16 enabled.");

    // read soft IP ID
   if (!(co_await readSoftIPId())) {
        lg2::error("Error: read soft IP ID failed.");
        co_return false;
    }

    if (!(co_await waitUntilReady(ReadyTimeout)))
    {
        lg2::error("Error: Device not ready.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::doErase()
{
    lg2::debug("Erasing {TARGET}...", "TARGET", target);
    if (!(co_await eraseCfg()))
    {
        lg2::error("Erase cfg data failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::doUpdate()
{
    lg2::debug("Programming {TARGET}...", "TARGET", target);
    if (!(co_await programCfg()))
    {
        lg2::error("Program cfg data failed.");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> LatticeXO5CPLD2::finishUpdate()
{
    if (!(co_await verifyCfg()))
    {
        lg2::error("Verify cfg data failed.");
        co_return false;
    }

    uint32_t userCode = 0;
    if (!(co_await readUserCode(userCode))) {
        lg2::error("Read usercode failed.");
        co_return false;
    }
    lg2::info("CPLD {TARGET} USERCODE: {USERCODE}", "TARGET", target,
               "USERCODE", lg2::hex, userCode);    
    
    co_return true;
}

} // namespace phosphor::software::cpld
