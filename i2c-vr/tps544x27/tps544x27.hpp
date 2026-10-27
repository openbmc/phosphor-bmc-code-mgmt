#pragma once

#include "common/include/i2c/i2c.hpp"
#include "csv_parser.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
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
    using CsvOp = TPS544X27CsvParser::CsvOp;
    using CsvCommand = TPS544X27CsvParser::CsvCommand;
    bool getStoreDelay(std::chrono::milliseconds& storeDelay) const;

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

    sdbusplus::async::task<bool> executeWriteByte(const CsvCommand& cmd);
    sdbusplus::async::task<bool> executeReadByte(const CsvCommand& cmd);
    sdbusplus::async::task<bool> executeWriteWord(const CsvCommand& cmd);
    sdbusplus::async::task<bool> executeReadWord(const CsvCommand& cmd);
    sdbusplus::async::task<bool> executeBlockWrite(const CsvCommand& cmd);
    sdbusplus::async::task<bool> executeBlockRead(const CsvCommand& cmd);
    sdbusplus::async::task<bool> executeBlockProcessCall(const CsvCommand& cmd);

    sdbusplus::async::task<bool> executeCsvCommandNoStore(
        const CsvCommand& cmd);

    sdbusplus::async::task<bool> executeCommandsBeforeStore();

    sdbusplus::async::task<bool> executeParsedImage();

    sdbusplus::async::task<bool> readDeviceRev(uint8_t* rev);

    sdbusplus::async::task<bool> readPasskeyChecksum(uint16_t* checksum);

    TPS544X27CsvParser csvParser;

    phosphor::i2c::I2C i2cInterface;

    uint16_t address;
};

} // namespace phosphor::software::VR
