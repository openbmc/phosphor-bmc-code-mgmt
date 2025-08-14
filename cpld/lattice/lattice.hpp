#pragma once
#include "common/include/i2c/i2c.hpp"

#include <chrono>
#include <iostream>
#include <string_view>
#include <utility>

struct cpldInfo
{
    std::string chipName;
    std::string updateStrategy;
    std::vector<uint8_t> deviceId;
};

const std::map<std::string, cpldInfo> supportedDeviceMap = {
    {"LatticeLCMXO3LF_2100CFirmware",
     {"LCMXO3LF-2100C", "XO2XO3FamilyUpdate", {0x61, 0x2b, 0xb0, 0x43}}},
    {"LatticeLCMXO3LF_4300CFirmware",
     {"LCMXO3LF-4300C", "XO2XO3FamilyUpdate", {0x61, 0x2b, 0xc0, 0x43}}},
    {"LatticeLCMXO3D_4300Firmware",
     {"LCMXO3D-4300", "XO2XO3FamilyUpdate", {0x01, 0x2e, 0x20, 0x43}}},
    {"LatticeLCMXO3D_9400Firmware",
     {"LCMXO3D-9400", "XO2XO3FamilyUpdate", {0x61, 0x2b, 0xe0, 0x43}}},
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
    commandEnableConfigMode = 0x74,
    commandReadFwVersion = 0xC0,
    commandProgramUserCode = 0xC2,
    commandReadDeviceId = 0xE0,
    commandReadBusyFlag = 0xF0,
};

constexpr std::chrono::milliseconds waitBusyTime(200);

class CpldLatticeManager
{
  public:
    CpldLatticeManager(sdbusplus::async::context& ctx, const uint16_t bus,
                       const uint8_t address, const std::string& chip,
                       const std::string& target, const bool debugMode) :
        ctx(ctx), chip(chip), target(target), debugMode(debugMode),
        i2cInterface(phosphor::i2c::I2C(bus, address))
    {}
    virtual ~CpldLatticeManager() = default;
    CpldLatticeManager(const CpldLatticeManager&) = delete;
    CpldLatticeManager& operator=(const CpldLatticeManager&) = delete;
    CpldLatticeManager(CpldLatticeManager&&) noexcept = delete;
    CpldLatticeManager& operator=(CpldLatticeManager&&) noexcept = delete;

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
    virtual sdbusplus::async::task<bool> readUserCode(uint32_t&) = 0;

    bool jedFileParser(const uint8_t* image, size_t imageSize);
    bool verifyChecksum();
    sdbusplus::async::task<bool> enableProgramMode();
    sdbusplus::async::task<bool> resetConfigFlash();
    sdbusplus::async::task<bool> programDone();
    sdbusplus::async::task<bool> disableConfigInterface();
    sdbusplus::async::task<bool> readBusyFlag(uint8_t& busyFlag);
    sdbusplus::async::task<bool> readStatusReg(uint8_t& statusReg);
    sdbusplus::async::task<bool> waitBusyAndVerify();
    static std::string uint32ToHexStr(uint32_t value);
};
