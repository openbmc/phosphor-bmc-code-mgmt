#pragma once
#include "common/include/i2c/i2c.hpp"

#include <chrono>
#include <iostream>
#include <string_view>
#include <utility>

struct cpldInfo
{
    std::string chipName;
    std::vector<uint8_t> deviceId;
};

const std::map<std::string, cpldInfo> supportedDeviceMap = {
    {"LatticeLCMXO3LF_2100CFirmware",
     {"LCMXO3LF-2100C", {0x61, 0x2b, 0xb0, 0x43}}},
    {"LatticeLCMXO3LF_4300CFirmware",
     {"LCMXO3LF-4300C", {0x61, 0x2b, 0xc0, 0x43}}},
    {"LatticeLCMXO3D_4300Firmware", {"LCMXO3D-4300", {0x01, 0x2e, 0x20, 0x43}}},
    {"LatticeLCMXO3D_9400Firmware", {"LCMXO3D-9400", {0x21, 0x2e, 0x30, 0x43}}},
};

struct cpldI2cInfo
{
    unsigned long int QF;
    unsigned int* UFM;
    unsigned int version;
    unsigned int checksum;
    std::vector<uint8_t> cfgData;
    std::vector<uint8_t> ufmData;
};

class CpldLatticeManager
{
  public:
    CpldLatticeManager(sdbusplus::async::context& ctx, const uint16_t bus,
                       const uint8_t address, const uint8_t* image,
                       size_t imageSize, const std::string& chip,
                       const std::string& target, const bool debugMode) :
        ctx(ctx), image(image), imageSize(imageSize), chip(chip),
        target(target), debugMode(debugMode),
        i2cInterface(phosphor::i2c::I2C(bus, address))
    {}
    sdbusplus::async::task<bool> updateFirmware(
        std::function<bool(int)> progressCallBack);
    sdbusplus::async::task<bool> getVersion(std::string& version);

  private:
    sdbusplus::async::context& ctx;
    cpldI2cInfo fwInfo{};
    const uint8_t* image;
    size_t imageSize;
    std::string chip;
    std::string target;
    std::vector<uint8_t> sumOnly;
    bool isLCMXO3D = false;
    bool debugMode = false;
    phosphor::i2c::I2C i2cInterface;

    sdbusplus::async::task<bool> XO2XO3FamilyUpdate(
        std::function<bool(int)> progressCallBack);

    int indexof(const char* str, const char* ptn);
    bool jedFileParser();
    bool verifyChecksum();
    sdbusplus::async::task<bool> readDeviceId();
    sdbusplus::async::task<bool> enableProgramMode();
    sdbusplus::async::task<bool> eraseFlash();
    sdbusplus::async::task<bool> resetConfigFlash();
    sdbusplus::async::task<bool> writeProgramPage();
    sdbusplus::async::task<bool> programUserCode();
    sdbusplus::async::task<bool> programDone();
    sdbusplus::async::task<bool> disableConfigInterface();
    sdbusplus::async::task<bool> readBusyFlag(uint8_t& busyFlag);
    sdbusplus::async::task<bool> readStatusReg(uint8_t& statusReg);
    sdbusplus::async::task<bool> waitBusyAndVerify();
    sdbusplus::async::task<bool> readUserCode(uint32_t& userCode);
};
