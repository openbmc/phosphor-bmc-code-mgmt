#include "mp2940x.hpp"

#include "common/include/host_power.hpp"
#include "common/include/utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <chrono>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace phosphor::software::VR
{

using namespace phosphor::software::host_power;

static constexpr size_t statusByteLength = 1;
static constexpr size_t pecLength = 1;

static constexpr uint8_t mp2940xPecConfigReg = 0x19;
static constexpr uint8_t mp2940xConfigRegFirst128 = 0x2B;
static constexpr uint8_t mp2940xConfigRegRemaining = 0x2A;

static constexpr uint8_t mp2940xCRCUserReg = 0x61;
static constexpr uint8_t mp2940xCRCMultiSelectReg = 0x03;
static constexpr uint8_t mp2940xCRCMultiReadReg = 0x41;
static constexpr uint8_t mp2940xCRCMulti1Index = 0x00;
static constexpr uint8_t mp2940xCRCMulti2Index = 0x01;

static uint32_t buildChecksum(uint16_t crcUser, uint16_t crcMulti1)
{
    return (static_cast<uint32_t>(crcUser) << 16) |
           static_cast<uint32_t>(crcMulti1);
}

sdbusplus::async::task<bool> MP2940X::verifyImage(const uint8_t* image,
                                                  size_t imageSize)
{
    /*
     * MP2940X ATE file is the 8-column Type1 ATE format with writeType.
     *
     * Use the common MPS Type1 parser so B4 rows are converted in the same
     * way as other MPS Type1 drivers:
     *
     *   ATE 6th column: A0003EFF
     *   parser data   : 04 FF 3E 00 A0
     *
     * The first byte is the PMBus block byte count. Do not add another
     * byte count when programming these parsed rows.
     */
    if (!co_await parseImage(image, imageSize, MPSImageType::type1))
    {
        error("MP2940X image verification failed: image parsing failed");
        co_return false;
    }

    if (configuration->registersData.empty())
    {
        error("MP2940X image verification failed: no configuration data found");
        co_return false;
    }

    if (configuration->configId == 0)
    {
        error("MP2940X image verification failed: missing config ID");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP2940X::parseDeviceConfiguration()
{
    if (!configuration)
    {
        error("MP2940X device configuration not initialized");
        co_return false;
    }

    if (!parser)
    {
        error("MP2940X image parser not initialized");
        co_return false;
    }

    size_t configRowCount = 0;

    for (const auto& tokens : parser->lineTokens)
    {
        if (tokens.empty())
        {
            continue;
        }

        if (tokens[0].starts_with("END"))
        {
            break;
        }

        if (!parser->isValidDataTokens(tokens))
        {
            continue;
        }

        /*
         * MP2940X AN240 Type1 ATE row format:
         *   0: configId
         *   1: pageNum
         *   2: regAddrHex
         *   3: regAddrDec
         *   4: regName
         *   5: regDataHex
         *   6: regDataDec
         *   7: writeType
         *
         * B4 rows are parsed by the common MPS Type1 parser as:
         *   data[0]    = block byte count
         *   data[1..4] = 6th column bytes in MPS parser order
         *
         * Example:
         *   A0003EFF / B4 -> 04 FF 3E 00 A0
         */
        if (tokens.size() < static_cast<size_t>(ATE::colCount))
        {
            error("MP2940X invalid token count: {COUNT}", "COUNT",
                  tokens.size());
            co_return false;
        }

        const auto page = parser->getVal<uint8_t>(tokens, ATE::pageNum);
        const auto reg = parser->getVal<uint8_t>(tokens, ATE::regAddrHex);

        /*
         * MP2940X configuration stream only uses Page3 0x2B / 0x2A B4 rows.
         * Ignore other rows before END only if they are not part of the stream.
         */
        if (page != static_cast<uint8_t>(MPSPage::page3))
        {
            debug("MP2940X skip non-page3 row, page {PAGE}", "PAGE", lg2::hex,
                  page);
            continue;
        }

        if (reg != mp2940xConfigRegFirst128 && reg != mp2940xConfigRegRemaining)
        {
            debug("MP2940X skip unsupported register {REG}", "REG", lg2::hex,
                  reg);
            continue;
        }

        if (configuration->configId == 0)
        {
            configuration->configId =
                parser->getVal<uint32_t>(tokens, ATE::configId);
        }

        ++configRowCount;
    }

    if (configRowCount == 0)
    {
        error("MP2940X no configuration rows found");
        co_return false;
    }

    if (configuration->registersData.empty())
    {
        error("MP2940X no Type1 parser data found");
        co_return false;
    }

    expectedConfigRowCount = configRowCount;
    info("MP2940X parsed {COUNT} configuration rows from Type1 parser", "COUNT",
         configRowCount);

    co_return true;
}

sdbusplus::async::task<bool> MP2940X::setPage(MPSPage page, bool withPEC)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf;

    tbuf = buildByteVector(PMBusCmd::page, page);

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MP2940X failed to set page {PAGE}, PEC {PEC}", "PAGE", lg2::hex,
              static_cast<uint8_t>(page), "PEC",
              withPEC ? "enabled" : "disabled");
        co_return false;
    }

    co_return true;
}

sdbusplus::async::task<bool> MP2940X::readByte(uint8_t command, uint8_t& value,
                                               bool withPEC)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf(
        withPEC ? statusByteLength + pecLength : statusByteLength);

    tbuf = {command};
    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MP2940X read byte failed, command {CMD}, PEC {PEC}", "CMD",
              lg2::hex, command, "PEC", withPEC ? "enabled" : "disabled");
        co_return false;
    }

    value = rbuf[0];
    co_return true;
}

sdbusplus::async::task<bool> MP2940X::readWord(uint8_t command, uint16_t& value,
                                               bool withPEC)
{
    std::vector<uint8_t> tbuf;
    std::vector<uint8_t> rbuf(withPEC ? 3 : 2);

    tbuf = {command};

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MP2940X read word failed, command {CMD}, PEC {PEC}", "CMD",
              lg2::hex, command, "PEC", withPEC ? "enabled" : "disabled");
        co_return false;
    }

    /*
     * PMBus Read Word returns low byte first.
     * Example:
     *   0xe9 0x09 -> 0x09E9
     */
    value = static_cast<uint16_t>(rbuf[0]) |
            (static_cast<uint16_t>(rbuf[1]) << 8);

    co_return true;
}

sdbusplus::async::task<bool> MP2940X::detectPECMode()
{
    /*
     * Before sending configuration data, read Page0 register 0x19 without PEC.
     * This implementation only supports non-PEC transfers.
     * If P0.19[7] indicates PEC mode, reject the update instead of forcing
     * non-PEC writes to a PEC-enabled device.
     */
    if (!co_await setPage(MPSPage::page0, false))
    {
        co_return false;
    }

    uint8_t value = 0;
    if (!co_await readByte(mp2940xPecConfigReg, value, false))
    {
        co_return false;
    }

    const bool devicePEC = (value & 0x80) != 0;
    if (devicePEC)
    {
        error(
            "MP2940X PEC mode is enabled, but only non-PEC update is currently supported");
        co_return false;
    }

    usePEC = false;
    info("MP2940X PEC mode: disabled");

    co_return true;
}

sdbusplus::async::task<bool> MP2940X::programConfigurationData()
{
    /*
     * Spec says MP2940X configuration data is sent through Page3.
     */
    if (!co_await setPage(MPSPage::page3, usePEC))
    {
        co_return false;
    }

    auto i2cWrite = [&](const std::vector<uint8_t>& tbufIn)
        -> sdbusplus::async::task<bool> {
        std::vector<uint8_t> tbuf = tbufIn;
        std::vector<uint8_t> rbuf;

        if (!i2cInterface.sendReceive(tbuf, rbuf))
        {
            error("MP2940X I2C write failed");
            co_return false;
        }

        co_return true;
    };

    /*
     * Vendor timing:
     * Every 128 rows are one programming unit.
     * Wait 40 ms after writing the first row of each unit.
     * Wait 3 ms after writing each remaining row in the same unit.
     */
    constexpr size_t rowsPerUnit = 128;
    constexpr auto firstRowOfUnitDelay = std::chrono::milliseconds(40);
    constexpr auto remainingRowDelay = std::chrono::milliseconds(3);

    size_t row = 0;

    for (const auto& data : configuration->registersData)
    {
        if (data.page != static_cast<uint8_t>(MPSPage::page3))
        {
            continue;
        }

        if (data.addr != mp2940xConfigRegFirst128 &&
            data.addr != mp2940xConfigRegRemaining)
        {
            continue;
        }

        if (data.length != 5 || data.data[0] != 0x04)
        {
            error("MP2940X invalid parsed B4 data at row {ROW}: "
                  "length {LEN}, byte_count {COUNT}",
                  "ROW", row + 1, "LEN", data.length, "COUNT", lg2::hex,
                  data.length > 0 ? data.data[0] : 0);
            co_return false;
        }

        const uint8_t expectedCommand =
            (row < firstConfigRows) ? mp2940xConfigRegFirst128
                                    : mp2940xConfigRegRemaining;

        if (data.addr != expectedCommand)
        {
            error("MP2940X unexpected register at row {ROW}: got {REG}, "
                  "expected {EXP}",
                  "ROW", row + 1, "REG", lg2::hex, data.addr, "EXP", lg2::hex,
                  expectedCommand);
            co_return false;
        }

        /*
         * Build PMBus Block Write buffer.
         *
         * data.addr:
         *   PMBus command code from ATE column 3.
         *   MP2940X config stream uses:
         *     0x2B for the first 128 rows
         *     0x2A for the remaining rows
         *
         * data.data:
         *   Parsed by common MPS Type1 parser from ATE column 6 and writeType.
         *   For B4 rows, data.data already includes the block byte count.
         *
         * Example ATE row:
         *   0012 3 2B 43 2B_4_20 A0003EFF 2684370687 B4
         *
         * Common parser result:
         *   data.addr   = 0x2B
         *   data.length = 5
         *   data.data   = 04 FF 3E 00 A0
         *
         * Final tbuf sent to I2C:
         *   tbuf[0] = data.addr      = 0x2B  // PMBus command
         *   tbuf[1] = data.data[0]   = 0x04  // block byte count
         *   tbuf[2] = data.data[1]   = 0xFF  // data byte 0, LSB first
         *   tbuf[3] = data.data[2]   = 0x3E  // data byte 1
         *   tbuf[4] = data.data[3]   = 0x00  // data byte 2
         *   tbuf[5] = data.data[4]   = 0xA0  // data byte 3, MSB
         *
         * Do not add another 0x04 here, because the common Type1 parser has
         * already inserted the block byte count for B4.
         */
        std::vector<uint8_t> tbuf;
        tbuf.reserve(static_cast<size_t>(data.length) + 1);
        tbuf.push_back(data.addr);
        tbuf.insert(tbuf.end(), data.data.begin(),
                    data.data.begin() + data.length);

        if (!co_await i2cWrite(tbuf))
        {
            error("MP2940X failed to write config row {ROW}, command {CMD}",
                  "ROW", row + 1, "CMD", lg2::hex, data.addr);
            co_return false;
        }

        const bool isFirstRowOfUnit = (row % rowsPerUnit) == 0;

        co_await sdbusplus::async::sleep_for(
            ctx, isFirstRowOfUnit ? firstRowOfUnitDelay : remainingRowDelay);

        ++row;
    }

    if (row == 0)
    {
        error("MP2940X no Page3 0x2B/0x2A rows programmed");
        co_return false;
    }

    if (row != expectedConfigRowCount)
    {
        error("MP2940X configuration row count mismatch: parsed {PARSED}, "
              "programmed {PROGRAMMED}",
              "PARSED", expectedConfigRowCount, "PROGRAMMED", row);
        co_return false;
    }

    info("MP2940X programmed {COUNT} configuration rows", "COUNT", row);

    co_return true;
}

sdbusplus::async::task<bool> MP2940X::updateFirmware(bool force)
{
    (void)force;

    auto powerState = co_await HostPower::getState(ctx);

    if (powerState != stateOff)
    {
        error(
            "MP2940X update rejected: host power must be off before VR update");
        co_return false;
    }

    PageGuard guard(i2cInterface);

    /*
     * verifyImage() is called before updateFirmware().
     * Keep this check here to avoid writing with empty parsed data.
     */
    if (!configuration || configuration->registersData.empty())
    {
        error("MP2940X update failed: image was not parsed");
        co_return false;
    }

    if (!co_await detectPECMode())
    {
        error("MP2940X failed to detect PEC mode");
        co_return false;
    }

    if (!co_await programConfigurationData())
    {
        error("MP2940X failed to program configuration data");
        co_return false;
    }

    bool inCRCBlock = false;
    bool foundCRCUser = false;
    bool foundCRCMulti1 = false;
    bool foundCRCMulti2 = false;
    uint16_t expectedCRCUser = 0;
    uint16_t expectedCRCMulti1 = 0;
    uint16_t expectedCRCMulti2 = 0;

    if (!parser)
    {
        error(
            "MP2940X failed to get expected checksum: parser not initialized");
        co_return false;
    }

    for (const auto& tokens : parser->lineTokens)
    {
        if (tokens.empty())
        {
            continue;
        }

        if (tokens[0].starts_with("CRC_CHECK_START"))
        {
            inCRCBlock = true;
            continue;
        }

        if (tokens[0].starts_with("CRC_CHECK_STOP"))
        {
            break;
        }

        if (!inCRCBlock)
        {
            continue;
        }

        if (tokens.size() < static_cast<size_t>(ATE::colCount))
        {
            continue;
        }

        std::string name = parser->getVal<std::string>(tokens, ATE::regName);
        if (!name.empty() && name.back() == '\r')
        {
            name.pop_back();
        }

        const auto value = parser->getVal<uint16_t>(tokens, ATE::regDataHex);

        if (name == "CRC_USER")
        {
            expectedCRCUser = value;
            foundCRCUser = true;
        }
        else if (name == "CRC_Multi_1")
        {
            expectedCRCMulti1 = value;
            foundCRCMulti1 = true;
        }
        else if (name == "CRC_Multi_2")
        {
            expectedCRCMulti2 = value;
            foundCRCMulti2 = true;
        }
    }

    if (!foundCRCUser || !foundCRCMulti1 || !foundCRCMulti2)
    {
        error("MP2940X failed to get expected checksum from image");
        co_return false;
    }

    const uint32_t expectedChecksum =
        buildChecksum(expectedCRCUser, expectedCRCMulti1);

    uint32_t actualChecksum = 0;
    if (!co_await getCRC(&actualChecksum))
    {
        error("MP2940X failed to get actual checksum");
        co_return false;
    }

    const uint16_t actualCRCUser =
        static_cast<uint16_t>((actualChecksum >> 16) & 0xFFFF);
    const uint16_t actualCRCMulti1 =
        static_cast<uint16_t>(actualChecksum & 0xFFFF);

    uint16_t actualCRCMulti2 = 0;

    if (!co_await setPage(MPSPage::page3, usePEC))
    {
        error("MP2940X failed to set Page3 for CRC_Multi_2 read");
        co_return false;
    }

    std::vector<uint8_t> tbuf = {
        mp2940xCRCMultiSelectReg,
        mp2940xCRCMulti2Index,
    };
    std::vector<uint8_t> rbuf;

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MP2940X failed to select CRC_Multi_2");
        co_return false;
    }

    if (!co_await readWord(mp2940xCRCMultiReadReg, actualCRCMulti2, usePEC))
    {
        error("MP2940X failed to read CRC_Multi_2");
        co_return false;
    }

    info("MP2940X checksum before AC: expected CRC_USER {EXPUSER}, "
         "CRC_Multi_1 {EXPMULTI1}, CRC_Multi_2 {EXPMULTI2}, "
         "checksum {EXPCHECKSUM}",
         "EXPUSER", lg2::hex, expectedCRCUser, "EXPMULTI1", lg2::hex,
         expectedCRCMulti1, "EXPMULTI2", lg2::hex, expectedCRCMulti2,
         "EXPCHECKSUM", lg2::hex, expectedChecksum);

    info("MP2940X checksum before AC: actual CRC_USER {ACTUSER}, "
         "CRC_Multi_1 {ACTMULTI1}, CRC_Multi_2 {ACTMULTI2}, "
         "checksum {ACTCHECKSUM}",
         "ACTUSER", lg2::hex, actualCRCUser, "ACTMULTI1", lg2::hex,
         actualCRCMulti1, "ACTMULTI2", lg2::hex, actualCRCMulti2, "ACTCHECKSUM",
         lg2::hex, actualChecksum);

    if (actualCRCUser != expectedCRCUser ||
        actualCRCMulti1 != expectedCRCMulti1 ||
        actualCRCMulti2 != expectedCRCMulti2)
    {
        error("MP2940X checksum mismatch before AC");
        co_return false;
    }

    info("MP2940X configuration write completed; checksum matched before AC");

    co_return true;
}

sdbusplus::async::task<bool> MP2940X::getCRC(uint32_t* checksum)
{
    if (checksum == nullptr)
    {
        error("MP2940X getCRC() called with null checksum pointer");
        co_return false;
    }

    PageGuard guard(i2cInterface);

    /*
     * This implementation currently supports only non-PEC transfers.
     * Reject CRC read if the device is configured for PEC mode.
     */
    if (!co_await setPage(MPSPage::page0, false))
    {
        error("MP2940X failed to set Page0 for PEC mode check");
        co_return false;
    }

    uint8_t pecConfig = 0;
    if (!co_await readByte(mp2940xPecConfigReg, pecConfig, false))
    {
        error("MP2940X failed to read PEC mode");
        co_return false;
    }

    if ((pecConfig & 0x80) != 0)
    {
        error("MP2940X PEC mode is enabled, but only non-PEC CRC read is "
              "currently supported");
        co_return false;
    }

    /*
     * Force non-PEC for CRC read.
     *
     * MP2940X version format:
     *   [31:16] = CRC_USER
     *   [15:0]  = CRC_Multi_1
     *
     * Example:
     *   CRC_USER    = 0x09E9
     *   CRC_Multi_1 = 0x8679
     *   Version     = 0x09E98679
     */
    usePEC = false;

    uint16_t crcUser = 0;
    uint16_t crcMulti1 = 0;
    /*
     * CRC_USER:
     *   Set Page2
     *   Read Word Page2 0x61
     */

    if (!co_await setPage(MPSPage::page2, usePEC))
    {
        error("MP2940X failed to set Page2 for CRC_USER read");
        co_return false;
    }

    if (!co_await readWord(mp2940xCRCUserReg, crcUser, usePEC))
    {
        error("MP2940X failed to read CRC_USER");
        co_return false;
    }

    /*
     * CRC_Multi_1:
     *   Set Page3
     *   Write Byte Page3 0x03 = 0x00
     *   Read Word Page3 0x41
     */
    if (!co_await setPage(MPSPage::page3, usePEC))
    {
        error("MP2940X failed to set Page3 for CRC_Multi_1 read");
        co_return false;
    }

    std::vector<uint8_t> tbuf = {
        mp2940xCRCMultiSelectReg,
        mp2940xCRCMulti1Index,
    };
    std::vector<uint8_t> rbuf;

    if (!i2cInterface.sendReceive(tbuf, rbuf))
    {
        error("MP2940X failed to select CRC_Multi_1");
        co_return false;
    }

    if (!co_await readWord(mp2940xCRCMultiReadReg, crcMulti1, usePEC))
    {
        error("MP2940X failed to read CRC_Multi_1");
        co_return false;
    }

    *checksum = (static_cast<uint32_t>(crcUser) << 16) |
                static_cast<uint32_t>(crcMulti1);

    info("MP2940X CRC_USER {CRCUSER}, CRC_Multi_1 {CRCMULTI1}, "
         "checksum {CHECKSUM}",
         "CRCUSER", lg2::hex, crcUser, "CRCMULTI1", lg2::hex, crcMulti1,
         "CHECKSUM", lg2::hex, *checksum);

    co_return true;
}

bool MP2940X::forcedUpdateAllowed()
{
    return true;
}

} // namespace phosphor::software::VR
