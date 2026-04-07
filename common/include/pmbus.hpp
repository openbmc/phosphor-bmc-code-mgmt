#pragma once

#include "common/include/i2c/i2c.hpp"

#include <cstdint>
#include <optional>

enum class PMBusCmd : uint8_t
{
    page = 0x00,
    writeProtect = 0x10,
    restoreUserAll = 0x16,
    storeUserCode = 0x17,
    statusCML = 0x7E,
    mfrId = 0x99,
    mfrModel = 0x9A,
    mfrSerial = 0x9E,
    icDeviceId = 0xAD,
};

class PageGuard
{
  public:
    PageGuard() = delete;
    PageGuard(const PageGuard&) = delete;
    PageGuard& operator=(const PageGuard&) = delete;

    /** @brief Constructor - Attempts to read and save the current page
     * @param[in] i2c - I2C interface to the device
     */
    explicit PageGuard(phosphor::i2c::I2C& i2cInterface);

    /** @brief Destructor - Restores the saved page if capture was successful */
    ~PageGuard();

  private:
    /** @brief Reference to the I2C interface */
    phosphor::i2c::I2C& i2cInterface;

    /** @brief The saved page value, nullopt if the initial read failed */
    std::optional<uint8_t> originalPage;
};
