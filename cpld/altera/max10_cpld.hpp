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
    Max10CPLD(sdbusplus::async::context& ctx, uint16_t bus, uint8_t address,
              const std::string& chip, const std::string& configType,
              bool debugMode, const Max10Profile& profile);

    ~Max10CPLD();

    Max10CPLD(const Max10CPLD&) = delete;
    Max10CPLD& operator=(const Max10CPLD&) = delete;
    Max10CPLD(Max10CPLD&&) = delete;
    Max10CPLD& operator=(Max10CPLD&&) = delete;

    sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progressCallback);

    sdbusplus::async::task<bool> getVersion(std::string& version);

  private:
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
                    const std::function<bool(int)>& progressCallback);

    static uint8_t bitReverse(uint8_t value);
    static uint32_t packWord(const uint8_t* data);
    static std::string toHex8(uint32_t value);

    uint16_t bus = 0;
    uint8_t address = 0;
    std::string chip;
    std::string configType;
    bool debugMode = false;
    Max10Profile profile{};
    int fd = -1;
};

} // namespace phosphor::software::cpld
