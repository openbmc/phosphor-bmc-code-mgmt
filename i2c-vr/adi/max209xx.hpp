#pragma once

#include "common/include/i2c/i2c.hpp"
#include "i2c-vr/vr.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace phosphor::software::VR
{

/**
 * @brief One PMBus register write parsed from an Intel HEX record: a
 *        register address followed by its little-endian data bytes.
 */
struct MAX209xxReg
{
    uint8_t addr = 0;
    std::vector<uint8_t> data;
};

/**
 * @brief All register writes belonging to a single rail (Rail A / Rail B)
 *        section of a MAX209xx PMBus register configuration file.
 */
struct MAX209xxRailConfig
{
    std::vector<MAX209xxReg> registers;

    // Target value of the Scenario_Control register (0xFB), i.e. the value
    // this rail's register list writes to 0xFB, if present.
    std::optional<uint8_t> scenarioTarget;
};

class MAX209XX : public VoltageRegulator
{
  public:
    MAX209XX(sdbusplus::async::context& ctx, uint16_t bus, uint16_t address) :
        VoltageRegulator(ctx), i2cInterface(phosphor::i2c::I2C(bus, address))
    {}

    sdbusplus::async::task<bool> verifyImage(const uint8_t* image,
                                             size_t imageSize) final;
    sdbusplus::async::task<bool> updateFirmware(bool force) final;
    sdbusplus::async::task<bool> getCRC(uint32_t* checksum) final;
    bool forcedUpdateAllowed() final;

  private:
    sdbusplus::async::task<bool> disableWriteProtect();
    sdbusplus::async::task<bool> programRail(const MAX209xxRailConfig& rail,
                                             uint8_t page);
    sdbusplus::async::task<bool> storeUserAll();
    sdbusplus::async::task<bool> verifyScenario(uint8_t page,
                                                uint8_t expected);

    phosphor::i2c::I2C i2cInterface;

    MAX209xxRailConfig railA;
    MAX209xxRailConfig railB;
};

} // namespace phosphor::software::VR
