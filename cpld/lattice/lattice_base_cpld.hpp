#pragma once
#include "common/include/i2c/i2c.hpp"

#include <phosphor-logging/lg2.hpp>

#include <chrono>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace phosphor::software::cpld
{

enum class latticeChip
{
    LCMXO2_4000HC,
    LCMXO3LF_2100C,
    LCMXO3LF_4300C,
    LCMXO3D_4300,
    LCMXO3D_9400,
    LFMXO5_25,
    LFMXO5_15D,
    UNSUPPORTED = -1,
};

enum class latticeStringType
{
    typeString,
    modelString,
};

inline std::string getLatticeChipStr(latticeChip chip,
                                     latticeStringType stringType)
{
    static const std::unordered_map<latticeChip, std::string> chipStringMap = {
        {latticeChip::LCMXO2_4000HC, "LCMXO2_4000HC"},
        {latticeChip::LCMXO3LF_2100C, "LCMXO3LF_2100C"},
        {latticeChip::LCMXO3LF_4300C, "LCMXO3LF_4300C"},
        {latticeChip::LCMXO3D_4300, "LCMXO3D_4300"},
        {latticeChip::LCMXO3D_9400, "LCMXO3D_9400"},
        {latticeChip::LFMXO5_25, "LFMXO5_25"},
        {latticeChip::LFMXO5_15D, "LFMXO5_15D"},
    };
    auto chipString = chipStringMap.at(chip);
    if (chipStringMap.find(chip) == chipStringMap.end())
    {
        lg2::error("Unsupported chip enum: {CHIPENUM}", "CHIPENUM", chip);
        return "";
    }

    switch (stringType)
    {
        case latticeStringType::typeString:
            return std::string("Lattice" + chipString + "Firmware");
        case latticeStringType::modelString:
            std::replace(chipString.begin(), chipString.end(), '_', '-');
            return chipString;
        default:
            lg2::error("Unsupported string type: {STRINGTYPE}", "STRINGTYPE",
                       stringType);
            return "";
    }
};

enum class latticeChipFamily
{
    XO2,
    XO3,
    XO5,
    XO5D,
};

struct cpldInfo
{
    latticeChipFamily chipFamily;
    std::vector<uint8_t> deviceId;
};

const std::map<latticeChip, cpldInfo> supportedDeviceMap = {
    {latticeChip::LCMXO2_4000HC,
     {latticeChipFamily::XO2, {0x01, 0x2b, 0xc0, 0x43}}},
    {latticeChip::LCMXO3LF_2100C,
     {latticeChipFamily::XO3, {0x61, 0x2b, 0xb0, 0x43}}},
    {latticeChip::LCMXO3LF_4300C,
     {latticeChipFamily::XO3, {0x61, 0x2b, 0xc0, 0x43}}},
    {latticeChip::LCMXO3D_4300,
     {latticeChipFamily::XO3, {0x01, 0x2e, 0x20, 0x43}}},
    {latticeChip::LCMXO3D_9400,
     {latticeChipFamily::XO3, {0x21, 0x2e, 0x30, 0x43}}},
    {latticeChip::LFMXO5_25, {latticeChipFamily::XO5, {}}},
    {latticeChip::LFMXO5_15D, {latticeChipFamily::XO5D, {}}},
};

struct cpldI2cInfo
{
    unsigned long int fuseQuantity;
    unsigned int* userFlashMemory;
    unsigned int version;
    unsigned int checksum;
    std::vector<uint8_t> cfgData;
    std::vector<uint8_t> ufmData;
};

enum cpldI2cCmd
{
    commandEraseFlash = 0x0E,
    commandDisableConfigInterface = 0x26,
    commandReadStatusReg = 0x3C,
    commandResetConfigFlash = 0x46,
    commandProgramDone = 0x5E,
    commandProgramPage = 0x70,
    commandReadPage = 0x73,
    commandEnableConfigMode = 0x74,
    commandSetPageAddress = 0xB4,
    commandReadFwVersion = 0xC0,
    commandProgramUserCode = 0xC2,
    commandReadDeviceId = 0xE0,
    commandReadBusyFlag = 0xF0,
};

constexpr std::chrono::milliseconds waitBusyTime(200);

class LatticeBaseCPLD
{
  public:
    LatticeBaseCPLD(sdbusplus::async::context& ctx, const uint16_t bus,
                    const uint8_t address, const std::string& chip,
                    const std::string& target, const bool debugMode) :
        ctx(ctx), chip(chip), target(target), debugMode(debugMode),
        i2cInterface(phosphor::i2c::I2C(bus, address))
    {}
    virtual ~LatticeBaseCPLD() = default;
    LatticeBaseCPLD(const LatticeBaseCPLD&) = delete;
    LatticeBaseCPLD& operator=(const LatticeBaseCPLD&) = delete;
    LatticeBaseCPLD(LatticeBaseCPLD&&) noexcept = delete;
    LatticeBaseCPLD& operator=(LatticeBaseCPLD&&) noexcept = delete;

    sdbusplus::async::task<bool> updateFirmware(
        const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progressCallBack);

    sdbusplus::async::task<bool> getVersion(std::string& version);

  protected:
    sdbusplus::async::context& ctx;
    cpldI2cInfo fwInfo{};
    std::string chip;
    std::string target;
    std::vector<uint8_t> sumOnly;
    bool isLCMXO3D = false;
    bool debugMode = false;
    phosphor::i2c::I2C i2cInterface;

    virtual sdbusplus::async::task<bool> prepareUpdate(const uint8_t*,
                                                       size_t) = 0;
    virtual sdbusplus::async::task<bool> doUpdate() = 0;
    virtual sdbusplus::async::task<bool> finishUpdate() = 0;

    bool jedFileParser(const uint8_t* image, size_t imageSize);
    bool verifyChecksum();
    sdbusplus::async::task<bool> enableProgramMode();
    sdbusplus::async::task<bool> resetConfigFlash();
    sdbusplus::async::task<bool> programDone();
    sdbusplus::async::task<bool> disableConfigInterface();
    sdbusplus::async::task<bool> waitBusyAndVerify();

  private:
    virtual sdbusplus::async::task<bool> readUserCode(uint32_t&) = 0;
    sdbusplus::async::task<bool> readBusyFlag(uint8_t& busyFlag);
    sdbusplus::async::task<bool> readStatusReg(uint8_t& statusReg);
    static std::string uint32ToHexStr(uint32_t value);
};

} // namespace phosphor::software::cpld
