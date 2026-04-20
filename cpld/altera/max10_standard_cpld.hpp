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
    // Default Avalon-MM IP Offsets
    uint32_t csrBase = 0x00200020;   // ON_CHIP_FLASH_IP_CSR_BASE
    uint32_t dataBase = 0x00000000;  // ON_CHIP_FLASH_IP_DATA_REG
    uint32_t bootBase = 0x00100000;  // DUAL_BOOT_IP_BASE
    uint32_t startAddr = 0x0004A000; // CFM0_10M16_START_ADDR
    uint32_t endAddr = 0x0008C000;   // CFM0_10M16_END_ADDR + 1 (exclusive)
    uint8_t imageType = 1;           // CFM_IMAGE_1
    bool littleEndian = true; // Typical endianness for this Avalon-MM bridge
};

class Max10StandardCPLD
{
  public:
    Max10StandardCPLD(sdbusplus::async::context& ctx, uint16_t bus,
                      uint8_t address, const std::string& chip,
                      const std::string& configType,
                      const Max10Profile& profile);

    ~Max10StandardCPLD();

    Max10StandardCPLD(const Max10StandardCPLD&) = delete;
    Max10StandardCPLD& operator=(const Max10StandardCPLD&) = delete;
    Max10StandardCPLD(Max10StandardCPLD&&) = delete;
    Max10StandardCPLD& operator=(Max10StandardCPLD&&) = delete;

    sdbusplus::async::task<bool> updateFirmware(
        bool force, const uint8_t* image, size_t imageSize,
        std::function<bool(int)> progressCallback);

    sdbusplus::async::task<bool> getVersion(std::string& version);

  private:
    bool openDevice();
    void closeDevice();

    // All hardware I/O and polling functions converted to async tasks
    sdbusplus::async::task<bool> readReg(uint32_t reg, uint32_t& value);
    sdbusplus::async::task<bool> writeReg(uint32_t reg, uint32_t value);
    sdbusplus::async::task<bool> readStatus(uint32_t& status);

    bool validateProfile() const;
    sdbusplus::async::task<bool> protectSectors();
    sdbusplus::async::task<bool> unprotectSector(int sectorId);
    sdbusplus::async::task<bool> eraseSector(int sectorId);
    sdbusplus::async::task<bool> waitEraseDone(int timeoutCount = 50000);
    sdbusplus::async::task<bool> waitWriteDone(int timeoutCount = 50000);
    sdbusplus::async::task<bool> programRpd(
        const uint8_t* image, size_t imageSize,
        const std::function<bool(int)>& progressCallback);

    static uint8_t bitReverse(uint8_t value);
    static uint32_t packWord(const uint8_t* data);

    sdbusplus::async::context& ctx;
    uint16_t bus = 0;
    uint8_t address = 0;
    std::string chip;
    std::string configType;
    Max10Profile profile{};
    int fd = -1;
};

} // namespace phosphor::software::cpld
