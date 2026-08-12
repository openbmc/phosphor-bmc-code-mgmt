#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace phosphor::software::VR
{

class TPS544X27 : public VoltageRegulator
{
  public:
    TPS544X27(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address)),
        address(address)
    {}

    sdbusplus::async::task<bool> getCRC(uint32_t* sum) final;

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;

    sdbusplus::async::task<bool> updateFirmware(bool force) final;

    bool forcedUpdateAllowed() final;

  private:
    enum class CsvOp
    {
        WriteByte,
        ReadByte,
        WriteWord,
        ReadWord,
        BlockWrite,
        BlockRead,
        BlockProcessCall,
        SendByte,
        Pause,
        Reset,
    };

    struct CsvCommand
    {
        CsvOp op = CsvOp::Reset;
        size_t lineNo = 0;

        uint8_t cmd = 0;

        // Write data without block length or PEC.
        std::vector<uint8_t> dataBytes;

        // Expected raw bytes from the CSV.
        std::vector<uint8_t> expectedBytes;

        bool hasWordValue = false;
        uint16_t wordValue = 0;

        bool hasCsvPEC = false;
        uint8_t csvPEC = 0;

        uint32_t pauseMs = 0;
    };

    struct CsvImage
    {
        bool pecEnabled = false;
        bool wordLowByteFirst = false;
        bool includeBlockLength = false;

        std::vector<CsvCommand> commands;
    };

    static const char* csvOpToString(CsvOp op);
    static bool parseCsvOp(const std::string& text, CsvOp& op);

    bool parseImage(const uint8_t* image, size_t imageSize,
                    std::string& errorMsg);

    static uint8_t calcPEC(const std::vector<uint8_t>& bytes);

    sdbusplus::async::task<bool> sendByte(uint8_t cmd);
    sdbusplus::async::task<bool> readByte(uint8_t cmd, uint8_t* value);

    sdbusplus::async::task<bool> readWordRaw(uint8_t cmd,
                                             std::vector<uint8_t>& raw);

    sdbusplus::async::task<bool> processCallReadWordRaw(
        uint8_t cmd, uint8_t low, uint8_t high, std::vector<uint8_t>& raw);

    sdbusplus::async::task<bool> blockRead(
        uint8_t cmd, std::vector<uint8_t>& data, size_t readLen);

    sdbusplus::async::task<bool> blockProcessCallRead(
        uint8_t cmd, const std::vector<uint8_t>& requestData,
        std::vector<uint8_t>& response, size_t readLen);

    sdbusplus::async::task<bool> executeCsvCommandNoStore(
        const CsvCommand& cmd);

    sdbusplus::async::task<bool> executeParsedImage();

    sdbusplus::async::task<bool> readDeviceRev(uint8_t* rev);

    sdbusplus::async::task<bool> readPasskeyChecksum(uint16_t* checksum);

    CsvImage parsedImage;

    phosphor::i2c::I2C i2cInterface;

    uint16_t address;
};

} // namespace phosphor::software::VR
