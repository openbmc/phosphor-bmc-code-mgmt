#pragma once
#include "cpld/altera/max10_base_cpld.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace phosphor::software::cpld
{

struct Max10Profile
{
    // Default values matching Facebook/Meta Clemente MAX10 Avalon-MM IP Offsets
    // (scm_cpld_attr)
    uint32_t csrBase = 0x00200020;   // ON_CHIP_FLASH_IP_CSR_BASE
    uint32_t dataBase = 0x00000000;  // ON_CHIP_FLASH_IP_DATA_REG
    uint32_t bootBase = 0x00100000;  // DUAL_BOOT_IP_BASE
    uint32_t startAddr = 0x0004A000; // CFM0_10M16_START_ADDR
    uint32_t endAddr = 0x0008C000;   // CFM0_10M16_END_ADDR + 1 (exclusive)
    uint8_t imageType = 1;           // CFM_IMAGE_1
    bool littleEndian = true; // Typical endianness for this Avalon-MM bridge
};

class Max10CPLD
{
  public:
    Max10CPLD(sdbusplus::async::context& ctx, const uint16_t bus,
              const uint8_t address, const std::string& chip,
              const std::string& target, const bool debugMode,
              const Max10Profile& profile);

    sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progressCallBack);

    sdbusplus::async::task<bool> getVersion(std::string& version);

  private:
    static constexpr uint32_t versionReg = 0x00100028;

    static constexpr uint32_t statusBusyErase = 0x01;
    static constexpr uint32_t statusBusyWrite = 0x02;
    static constexpr uint32_t statusBusyRead = 0x03;
    static constexpr uint32_t statusReadSuccess = 0x04;
    static constexpr uint32_t statusWriteSuccess = 0x08;
    static constexpr uint32_t statusEraseSuccess = 0x10;
    static constexpr uint32_t statusMask = 0x1F;

    static constexpr uint32_t protectSec5 = 0x1u << 27;
    static constexpr uint32_t protectSec4 = 0x1u << 26;
    static constexpr uint32_t protectSec3 = 0x1u << 25;
    static constexpr uint32_t protectSec2 = 0x1u << 24;
    static constexpr uint32_t protectSec1 = 0x1u << 23;

    static constexpr uint32_t sectorErase5 = 0b101u << 20;
    static constexpr uint32_t sectorErase4 = 0b100u << 20;
    static constexpr uint32_t sectorErase3 = 0b011u << 20;
    static constexpr uint32_t sectorErase2 = 0b010u << 20;
    static constexpr uint32_t sectorErase1 = 0b001u << 20;
    static constexpr uint32_t sectorEraseNone = 0b111u << 20;

    bool ensureOpen();
    void closeDevice();

    bool readReg(uint32_t reg, uint32_t& value);
    bool writeReg(uint32_t reg, uint32_t value);
    bool readStatus(uint32_t& status);

    bool validateProfile() const;
    bool protectSectors();
    bool unprotectSector(int sectorId);
    bool eraseSector(int sectorId);
    bool waitEraseDone(int timeoutCount = 50000);
    bool waitWriteDone(int timeoutCount = 50000);
    bool programRpd(const uint8_t* image, size_t imageSize,
                    std::function<bool(int)> progressCallBack);

    static uint8_t bitReverse(uint8_t v);
    static uint32_t packWord(const uint8_t* p);
    static std::string toHex8(uint32_t v);

    // Retrieve the GPIO name from Entity-Manager
    std::string getUpdateGpioNameFromDBus() const;

    sdbusplus::async::context& ctx_;
    uint16_t bus_ = 0;
    uint8_t address_ = 0;
    std::string chip_;
    std::string target_;
    bool debugMode_ = false;
    Max10Profile profile_{};
    int fd_ = -1;
};

} // namespace phosphor::software::cpld
