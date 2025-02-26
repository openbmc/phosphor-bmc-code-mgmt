#pragma once
#include "common/include/i2c/i2c.hpp"

#include <chrono>
#include <iostream>
#include <string_view>

enum cpldI2cCmd
{
    CMD_ERASE_FLASH = 0x0E,
    CMD_DISABLE_CONFIG_INTERFACE = 0x26,
    CMD_READ_STATUS_REG = 0x3C,
    CMD_RESET_CONFIG_FLASH = 0x46,
    CMD_PROGRAM_DONE = 0x5E,
    CMD_PROGRAM_PAGE = 0x70,
    CMD_ENABLE_CONFIG_MODE = 0x74,
    CMD_READ_FW_VERSION = 0xC0,
    CMD_PROGRAM_USER_CODE = 0xC2,
    CMD_READ_DEVICE_ID = 0xE0,
    CMD_READ_BUSY_FLAG = 0xF0,
};

struct cpldI2cInfo
{
    unsigned long int QF;
    unsigned int* UFM;
    unsigned int Version;
    unsigned int CheckSum;
    std::vector<uint8_t> cfgData;
    std::vector<uint8_t> ufmData;
};

class CpldLatticeManager
{
  public:
    CpldLatticeManager(const uint16_t bus, const uint8_t address,
                       const uint8_t* image, size_t imageSize,
                       const std::string& chip, const std::string& target,
                       const bool debugMode) :
        bus(bus), addr(address), image(image), imageSize(imageSize), chip(chip),
        target(target), debugMode(debugMode),
        i2cInterface(phosphor::i2c::I2C(bus, address))
    {}
    sdbusplus::async::task<bool> fwUpdate();
    sdbusplus::async::task<bool> getVersion();
    sdbusplus::async::task<bool> XO2XO3Family_update();
    sdbusplus::async::task<bool> XO5Family_update();

  protected:
    cpldI2cInfo fwInfo{};
    uint16_t bus;
    uint8_t addr;
    const uint8_t* image;
    size_t imageSize;
    std::string chip;
    std::string target;
    bool isLCMXO3D = false;
    bool debugMode = false;
    phosphor::i2c::I2C i2cInterface;
    sdbusplus::async::task<bool> sendReceive(
        std::vector<uint8_t>& writeData, size_t readSize,
        std::vector<uint8_t>& readData) const;

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

class XO5I2CManager : public CpldLatticeManager
{
    friend class CpldLatticeManager;

  public:
    XO5I2CManager(const uint16_t bus, const uint8_t addr, const uint8_t* image,
                  size_t imageSize, const std::string& chip,
                  const std::string& target, const bool debugMode) :
        CpldLatticeManager(bus, addr, image, imageSize, chip, target, debugMode)
    {}

  private:
    bool XO5jedFileParser();
    sdbusplus::async::task<bool> set_page(uint8_t partition_num,
                                          uint32_t page_num);
    sdbusplus::async::task<bool> erase_flash(uint8_t size);
    sdbusplus::async::task<bool> cfg_reset_addr();
    sdbusplus::async::task<std::vector<uint8_t>> cfg_read_page();
    sdbusplus::async::task<bool> cfg_write_page(
        const std::vector<uint8_t>& byte_list);
    sdbusplus::async::task<bool> programCfgData(uint8_t cfg);
    sdbusplus::async::task<bool> verifyCfgData(uint8_t cfg);
};
