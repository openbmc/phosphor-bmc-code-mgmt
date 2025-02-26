#pragma once
#include "common/include/i2c/i2c.hpp"

#include <chrono>
#include <iostream>
#include <string_view>
#include <utility>

struct cpldI2cInfo
{
    unsigned long int QF; // Quantity of Fuses
    unsigned int* UFM;    // User Flash Memory
    unsigned int version;
    unsigned int checksum;
    std::vector<uint8_t> cfgData;
    std::vector<uint8_t> ufmData;
};

class CpldLatticeManager
{
  public:
    CpldLatticeManager(const uint16_t bus, const uint8_t address,
                       const uint8_t* image, size_t imageSize,
                       const std::string& chip, const std::string& target,
                       const bool debugMode,
                       std::function<bool(int)> progressCallBack) :
        image(image), imageSize(imageSize), chip(chip), target(target),
        debugMode(debugMode), progressCallBack(std::move(progressCallBack)),
        i2cInterface(phosphor::i2c::I2C(bus, address))
    {}
    sdbusplus::async::task<bool> updateFirmware();
    sdbusplus::async::task<bool> getVersion();
    sdbusplus::async::task<bool> XO2XO3FamilyUpdate();

  protected:
    cpldI2cInfo fwInfo{};
    const uint8_t* image;
    size_t imageSize;
    std::string chip;
    std::string target;
    bool isLCMXO3D = false;
    bool debugMode = false;
    std::function<bool(int)> progressCallBack;
    phosphor::i2c::I2C i2cInterface;
    sdbusplus::async::task<bool> sendrecv(
        std::vector<uint8_t>& writeData, size_t readSize,
        std::vector<uint8_t>& readData) const noexcept;

  private:
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
