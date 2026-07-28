#include "max209xx.hpp"

#include "common/include/pmbus.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <charconv>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

namespace
{

enum class MAX209xxCmd : uint8_t
{
    crc32 = 0xC8,
};

constexpr uint8_t pageRailA = 0x00;
constexpr uint8_t pageRailB = 0x01;
constexpr size_t crcBlockDataLen = 8;
constexpr size_t crcByteLen = 4;
constexpr uint8_t scenarioControlReg = 0xFB;

enum class RailSelector
{
    none,
    railA,
    railB,
};

enum class IntelHexRecordType : uint8_t
{
    data = 0x00,
    endOfFile = 0x01,
};

// One decoded Intel HEX record: ":llaaaatt[dd...]cc".
struct IntelHexRecord
{
    IntelHexRecordType type;
    uint8_t addr;
    std::vector<uint8_t> data;
};

std::optional<IntelHexRecord> parseIntelHexLine(std::string_view line)
{
    if (line.empty() || line.front() != ':')
    {
        return std::nullopt;
    }
    line.remove_prefix(1);

    auto commentPos = line.find("//");
    if (commentPos != std::string_view::npos)
    {
        line = line.substr(0, commentPos);
    }
    while (!line.empty() &&
           std::isspace(static_cast<unsigned char>(line.back())) != 0)
    {
        line.remove_suffix(1);
    }

    static constexpr size_t minHexChars = 10; // ll aaaa tt cc
    if (line.size() < minHexChars || (line.size() % 2) != 0)
    {
        return std::nullopt;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(line.size() / 2);
    for (size_t i = 0; i < line.size(); i += 2)
    {
        uint8_t byte = 0;
        auto [ptr, ec] =
            std::from_chars(line.data() + i, line.data() + i + 2, byte, 16);
        if (ec != std::errc())
        {
            return std::nullopt;
        }
        bytes.push_back(byte);
    }

    static constexpr size_t byteCountIdx = 0; // ll
    static constexpr size_t addrHiIdx = 1;    // aaaa (high byte)
    static constexpr size_t addrLoIdx = 2;    // aaaa (low byte)
    static constexpr size_t typeIdx = 3;      // tt
    static constexpr size_t dataStartIdx = 4; // dd...
    // ll + aaaa(2) + tt + cc, not counting the dd bytes themselves
    static constexpr size_t overheadBytes = 5;

    if (bytes.size() < overheadBytes)
    {
        return std::nullopt;
    }

    uint8_t byteCount = bytes[byteCountIdx]; // ll
    if (bytes.size() != static_cast<size_t>(byteCount) + overheadBytes)
    {
        return std::nullopt;
    }

    uint8_t checksum = 0;
    for (const auto& b : bytes)
    {
        checksum += b;
    }
    if (checksum != 0)
    {
        error("MAX209XX: Intel HEX checksum mismatch");
        return std::nullopt;
    }

    IntelHexRecord record;
    record.type = static_cast<IntelHexRecordType>(bytes[typeIdx]);
    record.addr =
        static_cast<uint8_t>((bytes[addrHiIdx] << 8) | bytes[addrLoIdx]);
    record.data.assign(bytes.begin() + dataStartIdx, bytes.end() - 1);
    return record;
}

void dumpRailRegisters(std::string_view rail,
                       const std::vector<MAX209xxReg>& registers)
{
    for (const auto& reg : registers)
    {
        std::string dataHex;
        for (auto b : reg.data)
        {
            dataHex += std::format("{:02X} ", b);
        }
        debug("MAX209XX:   {RAIL} {REG} = {DATA}", "RAIL", rail, "REG",
              lg2::hex, reg.addr, "DATA", dataHex);
    }
}

} // namespace

sdbusplus::async::task<bool> MAX209XX::verifyImage(const uint8_t* image,
                                                   size_t imageSize)
{
    railA = MAX209xxRailConfig{};
    railB = MAX209xxRailConfig{};

    std::string_view content(reinterpret_cast<const char*>(image), imageSize);

    RailSelector currentRail = RailSelector::none;

    size_t pos = 0;
    while (pos < content.size())
    {
        auto newlinePos = content.find('\n', pos);
        std::string_view line = (newlinePos == std::string_view::npos)
                                    ? content.substr(pos)
                                    : content.substr(pos, newlinePos - pos);
        pos = (newlinePos == std::string_view::npos)
                  ? content.size()
                  : newlinePos + 1;

        while (!line.empty() && (line.back() == '\r'))
        {
            line.remove_suffix(1);
        }

        if (line.empty())
        {
            continue;
        }

        if (line.starts_with("//"))
        {
            if (line.find("RAIL A") != std::string_view::npos)
            {
                currentRail = RailSelector::railA;
            }
            else if (line.find("RAIL B") != std::string_view::npos)
            {
                currentRail = RailSelector::railB;
            }
            continue;
        }

        if (line.front() != ':')
        {
            continue;
        }

        std::optional<IntelHexRecord> record = parseIntelHexLine(line);
        if (!record.has_value())
        {
            error("MAX209XX: Failed to parse Intel HEX record");
            co_return false;
        }

        if (record->type == IntelHexRecordType::endOfFile)
        {
            break;
        }

        if (currentRail == RailSelector::none)
        {
            error("MAX209XX: Register record found before a RAIL section");
            co_return false;
        }

        MAX209xxRailConfig& rail =
            (currentRail == RailSelector::railA) ? railA : railB;
        if (record->addr == scenarioControlReg && !record->data.empty())
        {
            rail.scenarioTarget = record->data[0];
        }
        rail.registers.push_back(
            MAX209xxReg{record->addr, std::move(record->data)});
    }

    if (railA.registers.empty())
    {
        error("MAX209XX: Image verification failed - no Rail A register "
              "data found");
        co_return false;
    }

    if (!railA.scenarioTarget.has_value())
    {
        error("MAX209XX: Image verification failed - Rail A is missing a "
              "Scenario_Control (0xFB) write");
        co_return false;
    }

    debug("MAX209XX: verify summary: Rail A registers={COUNTA} "
          "scenario(0xFB)={SCENA} | Rail B registers={COUNTB} "
          "scenario(0xFB)={SCENB}",
          "COUNTA", railA.registers.size(), "SCENA", lg2::hex,
          railA.scenarioTarget.value(), "COUNTB", railB.registers.size(),
          "SCENB", lg2::hex, railB.scenarioTarget.value_or(0xFF));

    dumpRailRegisters("A", railA.registers);
    dumpRailRegisters("B", railB.registers);

    co_return true;
}

sdbusplus::async::task<bool> MAX209XX::disableWriteProtect()
{
    std::vector<uint8_t> tbuf =
        buildByteVector(PMBusCmd::writeProtect, uint8_t(0x00));
    std::vector<uint8_t> rbuf;
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to disable WRITE_PROTECT");
        co_return false;
    }
    co_return true;
}

sdbusplus::async::task<bool> MAX209XX::programRail(
    const MAX209xxRailConfig& rail, uint8_t page)
{
    std::vector<uint8_t> tbuf = buildByteVector(PMBusCmd::page, page);
    std::vector<uint8_t> rbuf;
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to select PAGE {PAGE}", "PAGE", page);
        co_return false;
    }

    for (const auto& reg : rail.registers)
    {
        tbuf.clear();
        tbuf.push_back(reg.addr);
        tbuf.insert(tbuf.end(), reg.data.begin(), reg.data.end());
        rbuf.clear();

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("MAX209XX: Failed to write register 0x{REG} on PAGE "
                  "{PAGE}",
                  "REG", lg2::hex, reg.addr, "PAGE", page);
            co_return false;
        }
    }

    co_return true;
}

sdbusplus::async::task<bool> MAX209XX::storeUserAll()
{
    constexpr uint16_t storeUserAllWaitMs = 1000;

    std::vector<uint8_t> tbuf = buildByteVector(PMBusCmd::page, pageRailA);
    std::vector<uint8_t> rbuf;
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to select PAGE 0 before STORE_USER_ALL");
        co_return false;
    }

    tbuf = buildByteVector(PMBusCmd::storeUserAll);
    rbuf.clear();
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to send STORE_USER_ALL command");
        co_return false;
    }

    co_await sdbusplus::async::sleep_for(
        ctx, std::chrono::milliseconds(storeUserAllWaitMs));

    co_return true;
}

sdbusplus::async::task<bool> MAX209XX::verifyScenario(uint8_t page,
                                                      uint8_t expected)
{
    std::vector<uint8_t> tbuf = buildByteVector(PMBusCmd::page, page);
    std::vector<uint8_t> rbuf;
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to select PAGE {PAGE} for scenario verify",
              "PAGE", page);
        co_return false;
    }

    tbuf = buildByteVector(scenarioControlReg);
    rbuf.resize(1);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to read Scenario_Control (0xFB) for "
              "verify");
        co_return false;
    }

    // Only 0xFB[7:4] is compared: STORE_USER_ALL auto-increments the
    // user scenario nibble 0xFB[3:0].
    constexpr uint8_t scenarioNibbleMask = 0xF0;
    if ((rbuf[0] & scenarioNibbleMask) != (expected & scenarioNibbleMask))
    {
        error("MAX209XX: Scenario_Control (0xFB[7:4]) mismatch after "
              "STORE_USER_ALL: expected={EXPECTED} actual={ACTUAL}",
              "EXPECTED", lg2::hex, expected, "ACTUAL", lg2::hex, rbuf[0]);
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MAX209XX::getCRC(uint32_t* checksum)
{
    // 0xC8 returns a 1-byte length prefix + 8 data bytes: [0-3]=dynamic
    // (RAM) CRC, [4-7]=NVM-backed CRC, which is used as the version here.
    std::vector<uint8_t> tbuf = buildByteVector(PMBusCmd::page, pageRailA);
    std::vector<uint8_t> rbuf;
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to select PAGE 0 for CRC read");
        co_return false;
    }

    tbuf = buildByteVector(MAX209xxCmd::crc32);
    rbuf.resize(1 + crcBlockDataLen);
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MAX209XX: Failed to read CRC32");
        co_return false;
    }

    auto crcBytes =
        std::span(rbuf).subspan(1 + crcBlockDataLen - crcByteLen, crcByteLen);
    *checksum = bytesToInt<uint32_t>(crcBytes);

    co_return true;
}

bool MAX209XX::forcedUpdateAllowed()
{
    return true;
}

sdbusplus::async::task<bool> MAX209XX::updateFirmware(bool force)
{
    (void)force;

    if (!co_await disableWriteProtect())
    {
        co_return false;
    }

    if (!co_await programRail(railA, pageRailA))
    {
        co_return false;
    }

    if (!railB.registers.empty())
    {
        if (!co_await programRail(railB, pageRailB))
        {
            co_return false;
        }
    }

    if (!co_await storeUserAll())
    {
        co_return false;
    }

    if (!co_await verifyScenario(pageRailA, railA.scenarioTarget.value()))
    {
        co_return false;
    }

    co_return true;
}

} // namespace phosphor::software::VR
