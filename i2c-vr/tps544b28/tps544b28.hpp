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

// TPS544B28 only. The TPS544A28 shares the register map but we have no
// configuration script for it, so we cannot assume the image format matches
// and it is deliberately not claimed here.
class TPS544B28 : public VoltageRegulator
{
  public:
    TPS544B28(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address)),
        address(address)
    {}

    sdbusplus::async::task<bool> getCRC(uint32_t* sum) final;

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;

    sdbusplus::async::task<bool> updateFirmware(bool force) final;

    bool forcedUpdateAllowed() final;

  private:
    using CsvOp = TPS544B28CsvParser::CsvOp;
    using CsvCommand = TPS544B28CsvParser::CsvCommand;

    // Raw PMBus transactions.
    sdbusplus::async::task<bool> sendByte(uint8_t cmd);
    sdbusplus::async::task<bool> writeBytes(uint8_t cmd,
                                            const std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> readBytes(uint8_t cmd, size_t readLen,
                                           std::vector<uint8_t>& data);
    sdbusplus::async::task<bool> readWord(uint8_t cmd, uint16_t* value);

    // Confirms the part is a TPS544B28 before anything is written.
    sdbusplus::async::task<bool> checkIcDeviceId();

    // Executes one parsed line. Read lines are executed and compared against
    // the expected bytes the script carries.
    sdbusplus::async::task<bool> executeCommand(const CsvCommand& cmd);

    // Walks the script up to STORE_USER_ALL. Everything after the Reset line
    // needs a power cycle first, so it is not executed here.
    sdbusplus::async::task<bool> executeUntilStore();

    sdbusplus::async::task<bool> storeUserAll();

    // Hold time after STORE_USER_ALL, worked out from the script. Returns
    // false when the script has no STORE_USER_ALL or asks for too short a
    // hold time.
    bool getStoreDelay(std::chrono::milliseconds& delay) const;

    TPS544B28CsvParser csvParser;

    // Set by verifyImage(), which the framework always runs before
    // updateFirmware().
    std::chrono::milliseconds storeDelay{0};

    phosphor::i2c::I2C i2cInterface;

    uint16_t address;
};

} // namespace phosphor::software::VR
