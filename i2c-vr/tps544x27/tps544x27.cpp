#include "tps544x27.hpp"

#include "common/include/host_power.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

using namespace phosphor::software::host_power;

namespace
{

std::string trim(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])))
    {
        ++b;
    }

    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
    {
        --e;
    }

    return s.substr(b, e - b);
}

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string cur;
    bool inQuote = false;

    for (char c : line)
    {
        if (c == '"')
        {
            inQuote = !inQuote;
            continue;
        }

        if (!inQuote && (c == ',' || c == '\t'))
        {
            fields.push_back(trim(cur));
            cur.clear();
            continue;
        }

        cur.push_back(c);
    }

    fields.push_back(trim(cur));

    while (!fields.empty() && fields.back().empty())
    {
        fields.pop_back();
    }

    return fields;
}

bool parseHexByte(const std::string& input, uint8_t& value)
{
    std::string s = trim(input);

    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
    {
        s = s.substr(2);
    }

    if (s.empty() || s.size() > 2)
    {
        return false;
    }

    unsigned int v = 0;
    std::stringstream ss;
    ss << std::hex << s;
    ss >> v;

    if (ss.fail() || v > 0xFF)
    {
        return false;
    }

    value = static_cast<uint8_t>(v);
    return true;
}

bool parseHexBytes(const std::string& input, std::vector<uint8_t>& out)
{
    std::string s = trim(input);

    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
    {
        s = s.substr(2);
    }

    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char c) { return std::isspace(c); }),
            s.end());

    if (s.empty() || (s.size() % 2) != 0)
    {
        return false;
    }

    out.clear();

    for (size_t i = 0; i < s.size(); i += 2)
    {
        std::string byteStr = s.substr(i, 2);

        unsigned int v = 0;
        std::stringstream ss;
        ss << std::hex << byteStr;
        ss >> v;

        if (ss.fail() || v > 0xFF)
        {
            return false;
        }

        out.push_back(static_cast<uint8_t>(v));
    }

    return true;
}

std::string byteToHex(uint8_t b)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setw(2)
        << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

std::string bytesToHex(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty())
    {
        return "-";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i != 0)
        {
            oss << " ";
        }

        oss << byteToHex(bytes[i]);
    }

    return oss.str();
}

bool bytesEqualWithSpecialMask(uint8_t cmd,
                               const std::vector<uint8_t>& expected,
                               const std::vector<uint8_t>& actual)
{
    if (expected.size() != actual.size())
    {
        info(
            "TPS544x27 compare size mismatch, cmd={CMD}, expected={EXPECTED}, actual={ACTUAL}",
            "CMD", lg2::hex, cmd, "EXPECTED", bytesToHex(expected), "ACTUAL",
            bytesToHex(actual));
        return false;
    }

    // PMBUS_ADDR 0xD2: bit15 is RESERVED R-0h.
    // Some CSV files contain raw 0E 8E, while the device reads back 0E 0E.
    // Treat them as equal after masking bit15.
    if (cmd == 0xD2 && expected.size() == 2)
    {
        uint16_t exp = static_cast<uint16_t>(expected[0]) |
                       (static_cast<uint16_t>(expected[1]) << 8);
        uint16_t act = static_cast<uint16_t>(actual[0]) |
                       (static_cast<uint16_t>(actual[1]) << 8);

        constexpr uint16_t maskReservedBit15 = 0x7FFF;

        if ((exp & maskReservedBit15) != (act & maskReservedBit15))
        {
            info(
                "TPS544x27 compare mismatch, cmd={CMD}, expected={EXPECTED}, actual={ACTUAL}",
                "CMD", lg2::hex, cmd, "EXPECTED", bytesToHex(expected),
                "ACTUAL", bytesToHex(actual));
            return false;
        }

        return true;
    }

    if (expected != actual)
    {
        info(
            "TPS544x27 compare mismatch, cmd={CMD}, expected={EXPECTED}, actual={ACTUAL}",
            "CMD", lg2::hex, cmd, "EXPECTED", bytesToHex(expected), "ACTUAL",
            bytesToHex(actual));
        return false;
    }

    return true;
}

bool isReadOnlyCommandPerSpec(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x24: // VOUT_MAX
        case 0x28: // VOUT_DROOP
        case 0x47: // IOUT_OC_FAULT_RESPONSE
        case 0x98: // PMBUS_REVISION
        case 0x99: // MFR_ID
        case 0xC8: // DIE_ID
                   // Spec lists Block Write/RW, but Data Validity says attempts
                   // to write DIE_ID are treated as invalid/unsupported data
                   // and reported via STATUS_CML IVD. Skip writes in update
                   // flow.
        case 0xDD: // VENDOR_ID
        case 0xDE: // VID_SETTING
        case 0xDF: // SVID_STATUS_1_2
        case 0xE0: // CAPABILITY_SVID
            return true;
        default:
            return false;
    }
}

const char* readOnlyCommandName(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x24:
            return "VOUT_MAX";
        case 0x28:
            return "VOUT_DROOP";
        case 0x47:
            return "IOUT_OC_FAULT_RESPONSE";
        case 0x98:
            return "PMBUS_REVISION";
        case 0x99:
            return "MFR_ID";
        case 0xC8:
            return "DIE_ID";
        case 0xDD:
            return "VENDOR_ID";
        case 0xDE:
            return "VID_SETTING";
        case 0xDF:
            return "SVID_STATUS_1_2";
        case 0xE0:
            return "CAPABILITY_SVID";
        default:
            return "UNKNOWN_READ_ONLY";
    }
}

bool isDangerousSecurityWrite(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x0E: // PASSKEY
            return true;
        default:
            return false;
    }
}

const char* dangerousSecurityCommandName(uint8_t cmd)
{
    switch (cmd)
    {
        case 0x0E:
            return "PASSKEY";
        default:
            return "UNKNOWN_SECURITY";
    }
}

} // namespace

constexpr uint8_t regPasskey = 0x0E;
constexpr uint8_t regStoreUserAll = 0x15;
constexpr uint8_t regRestoreUserAll = 0x16;
constexpr uint8_t regICDeviceRev = 0xAE;

constexpr std::chrono::milliseconds storeRestoreDelay{1000};

uint8_t TPS544X27::calcPEC(const std::vector<uint8_t>& bytes) const
{
    uint8_t crc = 0x00;

    for (uint8_t byte : bytes)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
        {
            if (crc & 0x80)
            {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x07);
            }
            else
            {
                crc = static_cast<uint8_t>(crc << 1);
            }
        }
    }

    return crc;
}

sdbusplus::async::task<bool> TPS544X27::sendByte(uint8_t cmd)
{
    std::vector<uint8_t> tbuf{cmd};
    std::vector<uint8_t> rbuf;

    // Do not append PEC for SendByte in Fusion CSV.
    // Example: SendByte,0x15
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544x27 SendByte failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::readByte(uint8_t cmd, uint8_t* value)
{
    if (value == nullptr)
    {
        error("TPS544x27 ReadByte invalid input");
        co_return false;
    }

    std::vector<uint8_t> tbuf{cmd};
    std::vector<uint8_t> rbuf(1);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544x27 ReadByte failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    *value = rbuf[0];
    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::readWord(uint8_t cmd, uint16_t* value)
{
    if (value == nullptr)
    {
        error("TPS544x27 ReadWord invalid input");
        co_return false;
    }

    std::vector<uint8_t> tbuf{cmd};
    std::vector<uint8_t> rbuf(2);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544x27 ReadWord failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    *value = static_cast<uint16_t>(rbuf[0]) |
             (static_cast<uint16_t>(rbuf[1])
              << 8); // Little-endian: low byte first, high byte second

    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::readWordRaw(uint8_t cmd,
                                                    std::vector<uint8_t>& raw)
{
    std::vector<uint8_t> tbuf{cmd};
    std::vector<uint8_t> rbuf(2);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544x27 ReadWordRaw failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    raw = std::move(rbuf);
    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::processCallReadWordRaw(
    uint8_t cmd, uint8_t low, uint8_t high, std::vector<uint8_t>& raw)
{
    std::vector<uint8_t> tbuf{cmd, low, high};
    std::vector<uint8_t> rbuf(2);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544x27 ProcessCall read word failed, cmd={CMD}", "CMD",
              lg2::hex, cmd);
        co_return false;
    }

    raw = std::move(rbuf);
    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::blockRead(
    uint8_t cmd, std::vector<uint8_t>& data, size_t readLen)
{
    std::vector<uint8_t> tbuf{cmd};
    std::vector<uint8_t> rbuf(readLen);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544x27 BlockRead failed, cmd={CMD}", "CMD", lg2::hex, cmd);
        co_return false;
    }

    data = std::move(rbuf);
    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::readDeviceRev(uint8_t* rev)
{
    // Block Read
    // 01 73
    if (rev == nullptr)
    {
        error("TPS544x27 readDeviceRev invalid input");
        co_return false;
    }

    std::vector<uint8_t> data;

    if (!co_await blockRead(regICDeviceRev, data, 2))
    {
        error("TPS544x27 failed to read IC_DEVICE_REV");
        co_return false;
    }

    if (data.size() != 2 || data[0] != 0x01)
    {
        error("TPS544x27 invalid IC_DEVICE_REV response");
        co_return false;
    }

    *rev = data[1];

    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::readPasskeyChecksum(uint16_t* checksum)
{
    if (checksum == nullptr)
    {
        error("TPS544x27 readPasskeyChecksum invalid input");
        co_return false;
    }

    std::vector<uint8_t> raw;

    // PASSKEY 0x0E is Block Read.
    // Raw response:
    //   raw[0] = byte count = 0x03
    //   raw[1] = PASSKEY status/reserved bits
    //   raw[2] = NVM_CHECKSUM high byte for Fusion/display
    //   raw[3] = NVM_CHECKSUM low byte for Fusion/display
    // Example: 03 00 44 ED -> checksum 0x44ED.
    if (!co_await blockRead(regPasskey, raw, 4))
    {
        error("TPS544x27 failed to read PASSKEY");
        co_return false;
    }

    if (raw.size() < 4 || raw[0] != 0x03)
    {
        error("TPS544x27 invalid PASSKEY response");
        co_return false;
    }

    info("TPS544x27 PASSKEY raw: {B0} {B1} {B2} {B3}", "B0", lg2::hex, raw[0],
         "B1", lg2::hex, raw[1], "B2", lg2::hex, raw[2], "B3", lg2::hex,
         raw[3]);

    *checksum = (static_cast<uint16_t>(raw[2]) << 8) |
                static_cast<uint16_t>(raw[3]);

    info("TPS544x27 NVM checksum: {CHECKSUM}", "CHECKSUM", lg2::hex, *checksum);

    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::getCRC(uint32_t* sum)
{
    if (sum == nullptr)
    {
        error("TPS544x27 getCRC invalid input");
        co_return false;
    }

    uint16_t checksum = 0;

    if (!co_await readPasskeyChecksum(&checksum))
    {
        error("TPS544x27 failed to get NVM checksum");
        co_return false;
    }

    *sum = static_cast<uint32_t>(checksum);
    co_return true;
}

const char* TPS544X27::csvOpToString(CsvOp op)
{
    switch (op)
    {
        case CsvOp::WriteByte:
            return "WriteByte";
        case CsvOp::ReadByte:
            return "ReadByte";
        case CsvOp::WriteWord:
            return "WriteWord";
        case CsvOp::ReadWord:
            return "ReadWord";
        case CsvOp::BlockWrite:
            return "BlockWrite";
        case CsvOp::BlockRead:
            return "BlockRead";
        case CsvOp::BlockProcessCall:
            return "BlockProcessCall";
        case CsvOp::SendByte:
            return "SendByte";
        case CsvOp::Pause:
            return "Pause";
        case CsvOp::Reset:
            return "Reset";
    }

    return "Unknown";
}

bool TPS544X27::parseCsvOp(const std::string& text, CsvOp& op)
{
    if (text == "WriteByte")
    {
        op = CsvOp::WriteByte;
        return true;
    }
    if (text == "ReadByte")
    {
        op = CsvOp::ReadByte;
        return true;
    }
    if (text == "WriteWord")
    {
        op = CsvOp::WriteWord;
        return true;
    }
    if (text == "ReadWord")
    {
        op = CsvOp::ReadWord;
        return true;
    }
    if (text == "BlockWrite")
    {
        op = CsvOp::BlockWrite;
        return true;
    }
    if (text == "BlockRead")
    {
        op = CsvOp::BlockRead;
        return true;
    }
    if (text == "BlockProcessCall")
    {
        op = CsvOp::BlockProcessCall;
        return true;
    }
    if (text == "SendByte")
    {
        op = CsvOp::SendByte;
        return true;
    }
    if (text == "Pause")
    {
        op = CsvOp::Pause;
        return true;
    }
    if (text == "Reset")
    {
        op = CsvOp::Reset;
        return true;
    }

    return false;
}

bool TPS544X27::parseImage(const uint8_t* image, size_t imageSize,
                           std::string& errorMsg)
{
    parsedImage = {};

    if (image == nullptr || imageSize == 0)
    {
        errorMsg = "empty image";
        return false;
    }

    std::string csvText(reinterpret_cast<const char*>(image), imageSize);
    std::istringstream iss(csvText);
    std::string line;
    size_t lineNo = 0;

    while (std::getline(iss, line))
    {
        ++lineNo;

        line = trim(line);
        if (line.empty())
        {
            continue;
        }

        auto fields = splitCsvLine(line);
        if (fields.empty())
        {
            continue;
        }

        auto fail = [&](const std::string& msg) {
            errorMsg = "CSV line " + std::to_string(lineNo) + ": " + msg +
                       " | " + line;
            info("TPS544x27 CSV parse failed: {ERR}", "ERR", errorMsg);
            return false;
        };

        if (fields[0] == "Comment")
        {
            if (line.find("PEC=True") != std::string::npos)
            {
                parsedImage.pecEnabled = true;
            }

            if (line.find("SMBusWordCommandBytesOrderAsLowByteFirst=True") !=
                std::string::npos)
            {
                parsedImage.wordLowByteFirst = true;
            }

            if (line.find("IncludeBlockLength=True") != std::string::npos)
            {
                parsedImage.includeBlockLength = true;
            }

            continue;
        }

        CsvOp op = CsvOp::Reset;
        if (!parseCsvOp(fields[0], op))
        {
            return fail("unknown operation");
        }

        CsvCommand cmd;
        cmd.op = op;
        cmd.lineNo = lineNo;

        auto parseCmdField = [&]() -> bool {
            if (fields.size() < 2)
            {
                return false;
            }

            return parseHexByte(fields[1], cmd.cmd);
        };

        switch (op)
        {
            case CsvOp::WriteByte:
            {
                if (!parseCmdField() || fields.size() < 3)
                {
                    return fail("invalid WriteByte");
                }

                std::vector<uint8_t> raw;
                if (!parseHexBytes(fields[2], raw) || raw.size() != 2)
                {
                    return fail("WriteByte data must be data + PEC");
                }

                cmd.dataBytes = {raw[0]};
                cmd.hasCsvPEC = true;
                cmd.csvPEC = raw[1];
                break;
            }

            case CsvOp::ReadByte:
            {
                if (!parseCmdField() || fields.size() < 3)
                {
                    return fail("invalid ReadByte");
                }

                if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                    cmd.expectedBytes.size() != 1)
                {
                    return fail("ReadByte expected must be 1 byte");
                }
                break;
            }

            case CsvOp::WriteWord:
            {
                if (!parseCmdField() || fields.size() < 3)
                {
                    return fail("invalid WriteWord");
                }

                std::vector<uint8_t> raw;
                if (!parseHexBytes(fields[2], raw) || raw.size() != 3)
                {
                    return fail("WriteWord data must be low + high + PEC");
                }

                const uint8_t lo = raw[0];
                const uint8_t hi = raw[1];

                cmd.dataBytes = {lo, hi};
                cmd.hasWordValue = true;
                cmd.wordValue = static_cast<uint16_t>(lo) |
                                (static_cast<uint16_t>(hi) << 8);

                cmd.hasCsvPEC = true;
                cmd.csvPEC = raw[2];
                break;
            }

            case CsvOp::ReadWord:
            {
                if (!parseCmdField() || fields.size() < 3)
                {
                    return fail("invalid ReadWord");
                }

                if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                    cmd.expectedBytes.size() != 2)
                {
                    return fail("ReadWord expected must be 2 bytes");
                }
                break;
            }

            case CsvOp::BlockWrite:
            {
                if (!parseCmdField() || fields.size() < 3)
                {
                    return fail("invalid BlockWrite");
                }

                std::vector<uint8_t> raw;
                if (!parseHexBytes(fields[2], raw) || raw.size() < 3)
                {
                    return fail("BlockWrite data must be length + data + PEC");
                }

                const uint8_t blockLen = raw[0];
                const size_t dataLen = raw.size() - 2; // remove length and PEC

                if (dataLen != blockLen)
                {
                    return fail("BlockWrite length mismatch");
                }

                // Keep only data bytes here, no length and no CSV PEC.
                cmd.dataBytes.assign(raw.begin() + 1, raw.end() - 1);

                cmd.hasCsvPEC = true;
                cmd.csvPEC = raw.back();
                break;
            }

            case CsvOp::BlockRead:
            {
                if (!parseCmdField() || fields.size() < 3)
                {
                    return fail("invalid BlockRead");
                }

                if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                    cmd.expectedBytes.empty())
                {
                    return fail("BlockRead expected empty");
                }

                const uint8_t blockLen = cmd.expectedBytes[0];
                const size_t dataLen = cmd.expectedBytes.size() - 1;

                if (dataLen != blockLen)
                {
                    return fail("BlockRead expected length mismatch");
                }

                break;
            }

            case CsvOp::BlockProcessCall:
            {
                if (fields.size() < 3)
                {
                    return fail("invalid BlockProcessCall");
                }

                std::vector<uint8_t> request;
                if (!parseHexBytes(fields[1], request) || request.size() < 2)
                {
                    return fail("BlockProcessCall request must be cmd + data");
                }

                cmd.cmd = request[0];
                cmd.dataBytes.assign(request.begin() + 1, request.end());

                if (!parseHexBytes(fields[2], cmd.expectedBytes) ||
                    cmd.expectedBytes.empty())
                {
                    return fail("BlockProcessCall expected empty");
                }

                const uint8_t blockLen = cmd.expectedBytes[0];
                const size_t dataLen = cmd.expectedBytes.size() - 1;

                if (dataLen != blockLen)
                {
                    return fail("BlockProcessCall expected length mismatch");
                }

                break;
            }

            case CsvOp::SendByte:
            {
                if (!parseCmdField())
                {
                    return fail("invalid SendByte");
                }
                break;
            }

            case CsvOp::Pause:
            {
                if (fields.size() < 2)
                {
                    return fail("invalid Pause");
                }

                try
                {
                    cmd.pauseMs = static_cast<uint32_t>(std::stoul(fields[1]));
                }
                catch (const std::exception&)
                {
                    return fail("invalid Pause value");
                }
                break;
            }

            case CsvOp::Reset:
            {
                break;
            }
        }

        parsedImage.commands.push_back(std::move(cmd));
    }

    return true;
}

sdbusplus::async::task<bool> TPS544X27::blockProcessCallRead(
    uint8_t cmd, const std::vector<uint8_t>& requestData,
    std::vector<uint8_t>& response, size_t readLen)
{
    if (requestData.size() > 255)
    {
        error("TPS544x27 BlockProcessCall request too large, cmd={CMD}", "CMD",
              lg2::hex, cmd);
        co_return false;
    }

    std::vector<uint8_t> tbuf;
    tbuf.reserve(requestData.size() + 2);

    tbuf.push_back(cmd);
    tbuf.push_back(static_cast<uint8_t>(requestData.size()));
    tbuf.insert(tbuf.end(), requestData.begin(), requestData.end());

    std::vector<uint8_t> rbuf(readLen);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("TPS544x27 BlockProcessCall read failed, cmd={CMD}", "CMD",
              lg2::hex, cmd);
        co_return false;
    }

    response = std::move(rbuf);
    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::executeCsvCommandNoStore(
    const CsvCommand& cmd)
{
    switch (cmd.op)
    {
        case CsvOp::WriteByte:
        {
            if (cmd.dataBytes.size() != 1)
            {
                error("TPS544x27 invalid WriteByte data, line={LINE}", "LINE",
                      cmd.lineNo);
                co_return false;
            }

            if (isReadOnlyCommandPerSpec(cmd.cmd))
            {
                info("TPS544x27 skip CSV WriteByte because spec says write is "
                     "not supported, line={LINE}, cmd={CMD}, name={NAME}",
                     "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "NAME",
                     readOnlyCommandName(cmd.cmd));
                co_return true;
            }

            if (isDangerousSecurityWrite(cmd.cmd))
            {
                error("TPS544x27 reject CSV WriteByte to security/passkey "
                      "command, line={LINE}, cmd={CMD}, name={NAME}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "NAME",
                      dangerousSecurityCommandName(cmd.cmd));
                co_return false;
            }

            if (!cmd.hasCsvPEC)
            {
                error("TPS544x27 missing CSV PEC for WriteByte, line={LINE}, "
                      "cmd={CMD}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return false;
            }

            std::vector<uint8_t> tbuf{cmd.cmd, cmd.dataBytes[0]};
            std::vector<uint8_t> pecInput{
                static_cast<uint8_t>((address << 1) | 0x00)};
            pecInput.insert(pecInput.end(), tbuf.begin(), tbuf.end());

            const uint8_t calculatedPEC = calcPEC(pecInput);
            if (calculatedPEC != cmd.csvPEC)
            {
                error("TPS544x27 CSV PEC mismatch for WriteByte, line={LINE}, "
                      "cmd={CMD}, csvPEC={CSV_PEC}, calculatedPEC={CALC_PEC}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "CSV_PEC",
                      lg2::hex, cmd.csvPEC, "CALC_PEC", lg2::hex,
                      calculatedPEC);
                co_return false;
            }

            tbuf.push_back(cmd.csvPEC);
            std::vector<uint8_t> rbuf;
            if (!i2cInterface.sendReceive(tbuf, rbuf))
            {
                error("TPS544x27 WriteByte failed after CSV PEC verification, "
                      "line={LINE}, cmd={CMD}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return false;
            }

            co_return true;
        }

        case CsvOp::ReadByte:
        {
            uint8_t actual = 0;

            if (!co_await readByte(cmd.cmd, &actual))
            {
                co_return false;
            }

            std::vector<uint8_t> actualBytes{actual};

            if (!bytesEqualWithSpecialMask(cmd.cmd, cmd.expectedBytes,
                                           actualBytes))
            {
                error("TPS544x27 ReadByte mismatch, line={LINE}, cmd={CMD}, "
                      "expected={EXPECTED}, actual={ACTUAL}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "EXPECTED",
                      bytesToHex(cmd.expectedBytes), "ACTUAL",
                      bytesToHex(actualBytes));
                co_return false;
            }

            co_return true;
        }

        case CsvOp::WriteWord:
        {
            if (!cmd.hasWordValue)
            {
                error("TPS544x27 invalid WriteWord data, line={LINE}", "LINE",
                      cmd.lineNo);
                co_return false;
            }

            if (isReadOnlyCommandPerSpec(cmd.cmd))
            {
                info("TPS544x27 skip CSV WriteWord because spec says write is "
                     "not supported, line={LINE}, cmd={CMD}, name={NAME}",
                     "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "NAME",
                     readOnlyCommandName(cmd.cmd));
                co_return true;
            }

            if (isDangerousSecurityWrite(cmd.cmd))
            {
                error("TPS544x27 reject CSV WriteWord to security/passkey "
                      "command, line={LINE}, cmd={CMD}, name={NAME}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "NAME",
                      dangerousSecurityCommandName(cmd.cmd));
                co_return false;
            }

            if (!cmd.hasCsvPEC || cmd.dataBytes.size() != 2)
            {
                error(
                    "TPS544x27 missing CSV PEC or invalid data for WriteWord, "
                    "line={LINE}, cmd={CMD}",
                    "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return false;
            }

            std::vector<uint8_t> tbuf{cmd.cmd, cmd.dataBytes[0],
                                      cmd.dataBytes[1]};
            std::vector<uint8_t> pecInput{
                static_cast<uint8_t>((address << 1) | 0x00)};
            pecInput.insert(pecInput.end(), tbuf.begin(), tbuf.end());

            const uint8_t calculatedPEC = calcPEC(pecInput);
            if (calculatedPEC != cmd.csvPEC)
            {
                error("TPS544x27 CSV PEC mismatch for WriteWord, line={LINE}, "
                      "cmd={CMD}, csvPEC={CSV_PEC}, calculatedPEC={CALC_PEC}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "CSV_PEC",
                      lg2::hex, cmd.csvPEC, "CALC_PEC", lg2::hex,
                      calculatedPEC);
                co_return false;
            }

            tbuf.push_back(cmd.csvPEC);
            std::vector<uint8_t> rbuf;
            if (!i2cInterface.sendReceive(tbuf, rbuf))
            {
                error("TPS544x27 WriteWord failed after CSV PEC verification, "
                      "line={LINE}, cmd={CMD}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return false;
            }

            co_return true;
        }

        case CsvOp::ReadWord:
        {
            std::vector<uint8_t> actual;

            if (cmd.cmd == 0xC4)
            {
                // ADV_TEL_BYTE 0xC4 is not a normal ReadWord readback.
                // It uses a Process Call style read:
                //   write:  C4 00 <adv_tel_address>
                //   read:   <adv_tel_address> <data>
                //
                // Fusion CSV still records it as:
                //   WriteWord,0xC4,0x0500...
                //   ReadWord,0xC4,0x0500
                //
                // For the ReadWord line, expectedBytes[0] is the Adv Tel
                // address and expectedBytes[1] is the expected data.
                if (cmd.expectedBytes.size() != 2)
                {
                    error("TPS544x27 invalid ADV_TEL_BYTE expected size, "
                          "line={LINE}, size={SIZE}",
                          "LINE", cmd.lineNo, "SIZE", cmd.expectedBytes.size());
                    co_return false;
                }

                const uint8_t advTelAddr = cmd.expectedBytes[0];

                if (!co_await processCallReadWordRaw(cmd.cmd, 0x00, advTelAddr,
                                                     actual))
                {
                    co_return false;
                }
            }
            else if (!co_await readWordRaw(cmd.cmd, actual))
            {
                co_return false;
            }

            if (!bytesEqualWithSpecialMask(cmd.cmd, cmd.expectedBytes, actual))
            {
                error("TPS544x27 ReadWord mismatch, line={LINE}, cmd={CMD}, "
                      "expected={EXPECTED}, actual={ACTUAL}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "EXPECTED",
                      bytesToHex(cmd.expectedBytes), "ACTUAL",
                      bytesToHex(actual));
                co_return false;
            }

            co_return true;
        }

        case CsvOp::BlockWrite:
        {
            if (isReadOnlyCommandPerSpec(cmd.cmd))
            {
                info("TPS544x27 skip CSV BlockWrite because spec says write is "
                     "not supported, line={LINE}, cmd={CMD}, name={NAME}",
                     "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "NAME",
                     readOnlyCommandName(cmd.cmd));
                co_return true;
            }

            if (isDangerousSecurityWrite(cmd.cmd))
            {
                error("TPS544x27 reject CSV BlockWrite to security/passkey "
                      "command, line={LINE}, cmd={CMD}, name={NAME}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "NAME",
                      dangerousSecurityCommandName(cmd.cmd));
                co_return false;
            }

            if (!cmd.hasCsvPEC)
            {
                error("TPS544x27 missing CSV PEC for BlockWrite, line={LINE}, "
                      "cmd={CMD}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return false;
            }

            if (cmd.dataBytes.size() > 255)
            {
                error("TPS544x27 BlockWrite data too large, line={LINE}, "
                      "cmd={CMD}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return false;
            }

            std::vector<uint8_t> tbuf{cmd.cmd};
            if (parsedImage.includeBlockLength)
            {
                tbuf.push_back(static_cast<uint8_t>(cmd.dataBytes.size()));
            }
            tbuf.insert(tbuf.end(), cmd.dataBytes.begin(), cmd.dataBytes.end());

            std::vector<uint8_t> pecInput{
                static_cast<uint8_t>((address << 1) | 0x00)};
            pecInput.insert(pecInput.end(), tbuf.begin(), tbuf.end());

            const uint8_t calculatedPEC = calcPEC(pecInput);
            if (calculatedPEC != cmd.csvPEC)
            {
                error("TPS544x27 CSV PEC mismatch for BlockWrite, line={LINE}, "
                      "cmd={CMD}, csvPEC={CSV_PEC}, calculatedPEC={CALC_PEC}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "CSV_PEC",
                      lg2::hex, cmd.csvPEC, "CALC_PEC", lg2::hex,
                      calculatedPEC);
                co_return false;
            }

            tbuf.push_back(cmd.csvPEC);
            std::vector<uint8_t> rbuf;
            if (!i2cInterface.sendReceive(tbuf, rbuf))
            {
                error("TPS544x27 BlockWrite failed after CSV PEC verification, "
                      "line={LINE}, cmd={CMD}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
                co_return false;
            }

            co_return true;
        }

        case CsvOp::BlockRead:
        {
            std::vector<uint8_t> actual;

            if (!co_await blockRead(cmd.cmd, actual, cmd.expectedBytes.size()))
            {
                co_return false;
            }

            if (!bytesEqualWithSpecialMask(cmd.cmd, cmd.expectedBytes, actual))
            {
                error("TPS544x27 BlockRead mismatch, line={LINE}, cmd={CMD}, "
                      "expected={EXPECTED}, actual={ACTUAL}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "EXPECTED",
                      bytesToHex(cmd.expectedBytes), "ACTUAL",
                      bytesToHex(actual));
                co_return false;
            }

            co_return true;
        }

        case CsvOp::BlockProcessCall:
        {
            std::vector<uint8_t> actual;

            if (!co_await blockProcessCallRead(cmd.cmd, cmd.dataBytes, actual,
                                               cmd.expectedBytes.size()))
            {
                co_return false;
            }

            if (!bytesEqualWithSpecialMask(cmd.cmd, cmd.expectedBytes, actual))
            {
                error("TPS544x27 BlockProcessCall mismatch, line={LINE}, "
                      "cmd={CMD}, expected={EXPECTED}, actual={ACTUAL}",
                      "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd, "EXPECTED",
                      bytesToHex(cmd.expectedBytes), "ACTUAL",
                      bytesToHex(actual));
                co_return false;
            }

            co_return true;
        }

        case CsvOp::SendByte:
        {
            info("TPS544x27 defer CSV SendByte to update flow, line={LINE}, "
                 "cmd={CMD}",
                 "LINE", cmd.lineNo, "CMD", lg2::hex, cmd.cmd);
            co_return true;
        }

        case CsvOp::Pause:
        {
            co_return true;
        }

        case CsvOp::Reset:
        {
            info("TPS544x27 skip Reset, line={LINE}", "LINE", cmd.lineNo);
            co_return true;
        }
    }

    error("TPS544x27 unknown CSV op, line={LINE}", "LINE", cmd.lineNo);
    co_return false;
}

sdbusplus::async::task<bool> TPS544X27::executeParsedImage()
{
    if (parsedImage.commands.empty())
    {
        error("TPS544x27 no parsed CSV image to execute");
        co_return false;
    }

    if (!parsedImage.wordLowByteFirst)
    {
        error("TPS544x27 unsupported CSV word byte order");
        co_return false;
    }

    if (!parsedImage.includeBlockLength)
    {
        error("TPS544x27 unsupported CSV without block length");
        co_return false;
    }

    if (!parsedImage.pecEnabled)
    {
        error("TPS544x27 CSV without PEC is not supported");
        co_return false;
    }

    auto powerState = co_await HostPower::getState(ctx);

    if (powerState != stateOff)
    {
        error(
            "TPS544x27 update rejected: host power must be off before VR update");
        co_return false;
    }

    uint8_t rev = 0;
    if (!co_await readDeviceRev(&rev))
    {
        co_return false;
    }

    info("TPS544x27 IC_DEVICE_REV={REV}", "REV", lg2::hex, rev);

    if ((rev & 0x0F) == 0x01)
    {
        error("TPS544x27 B0 silicon requires the special STORE_USER_ALL "
              "recovery procedure, which is not supported, rev={REV}",
              "REV", lg2::hex, rev);
        co_return false;
    }

    bool hasStoreUserAll = false;
    std::chrono::milliseconds storeDelay = storeRestoreDelay;

    for (size_t i = 0; i < parsedImage.commands.size(); ++i)
    {
        const auto& cmd = parsedImage.commands[i];

        if (cmd.op == CsvOp::SendByte && cmd.cmd == regStoreUserAll)
        {
            hasStoreUserAll = true;

            if ((i + 1) < parsedImage.commands.size() &&
                parsedImage.commands[i + 1].op == CsvOp::Pause)
            {
                storeDelay = std::chrono::milliseconds(
                    parsedImage.commands[i + 1].pauseMs);

                constexpr std::chrono::milliseconds minimumDelay{150};

                if (storeDelay < minimumDelay)
                {
                    error("TPS544x27 CSV delay is below minimum, "
                          "delayMs={DELAY_MS}, minimumMs={MINIMUM_MS}",
                          "DELAY_MS", storeDelay.count(), "MINIMUM_MS",
                          minimumDelay.count());
                    co_return false;
                }
                info("TPS544x27 use CSV Pause after STORE_USER_ALL, "
                     "line={LINE}, delayMs={DELAY_MS}",
                     "LINE", parsedImage.commands[i + 1].lineNo, "DELAY_MS",
                     parsedImage.commands[i + 1].pauseMs);
            }
            else
            {
                info("TPS544x27 no CSV Pause after STORE_USER_ALL, "
                     "use default delayMs={DELAY_MS}",
                     "DELAY_MS", storeDelay.count());
            }

            break;
        }
    }

    if (!hasStoreUserAll)
    {
        error("TPS544x27 CSV does not contain STORE_USER_ALL SendByte 0x15");
        co_return false;
    }

    for (const auto& cmd : parsedImage.commands)
    {
        if (cmd.op == CsvOp::SendByte && cmd.cmd == regStoreUserAll)
        {
            info(
                "TPS544x27 stop CSV execution before STORE_USER_ALL, line={LINE}",
                "LINE", cmd.lineNo);
            break;
        }

        if (!co_await executeCsvCommandNoStore(cmd))
        {
            error(
                "TPS544x27 failed executing CSV line {LINE}, op={OP}, cmd={CMD}",
                "LINE", cmd.lineNo, "OP", csvOpToString(cmd.op), "CMD",
                lg2::hex, cmd.cmd);
            co_return false;
        }
    }

    info("TPS544x27 execute STORE_USER_ALL, delayMs={DELAY_MS}", "DELAY_MS",
         storeDelay.count());

    if (!co_await sendByte(regStoreUserAll))
    {
        error("TPS544x27 STORE_USER_ALL failed");
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(ctx, storeDelay);

    if (!co_await sendByte(regRestoreUserAll))
    {
        error("TPS544x27 RESTORE_USER_ALL failed");
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(ctx, storeDelay);

    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::verifyImage(const uint8_t* image,
                                                    size_t imageSize)
{
    std::string err;

    if (!parseImage(image, imageSize, err))
    {
        info("TPS544x27 image parse failed: {ERR}", "ERR", err);
        co_return false;
    }

    info("TPS544x27 image parsed, commands={COUNT}, PEC={PEC}", "COUNT",
         parsedImage.commands.size(), "PEC", parsedImage.pecEnabled);

    co_return true;
}

sdbusplus::async::task<bool> TPS544X27::updateFirmware(bool force)
{
    (void)force;

    info("TPS544x27 updateFirmware real read/write store mode");

    if (!co_await executeParsedImage())
    {
        error("TPS544x27 store update failed");
        co_return false;
    }

    info("TPS544x27 updateFirmware store mode passed");
    co_return true;
}

bool TPS544X27::forcedUpdateAllowed()
{
    return true;
}
} // namespace phosphor::software::VR
