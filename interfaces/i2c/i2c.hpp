#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

namespace i2c
{

class I2CException : public std::exception
{
  public:
    explicit I2CException(const std::string& info, const std::string& bus,
                          uint8_t addr, int errorCode = 0) :
        bus(bus), addr(addr), errorCode(errorCode)
    {
        std::stringstream ss;
        ss << "I2CException: " << info << ": bus " << bus << ", addr 0x"
           << std::hex << static_cast<int>(addr);
        if (errorCode != 0)
        {
            ss << std::dec << ", errno " << errorCode << ": "
               << strerror(errorCode);
        }
        errStr = ss.str();
    }

    virtual ~I2CException() = default;

    const char* what() const noexcept override
    {
        return errStr.c_str();
    }
    std::string bus;
    uint8_t addr;
    int errorCode;
    std::string errStr;
};

class I2C
{
  private:
    static constexpr int INVALID_FD = -1;
    int fd = INVALID_FD;
    std::string busStr;
    uint8_t device_address;
    int max_retries;

    void open();

    int checkIsOpen() const
    {
        if (!isOpen())
        {
            return -1;
        }
        return 0;
    }

    void closeWithoutException() noexcept
    {
        try
        {
            close();
        }
        catch (...)
        {}
    }

  public:
    enum class InitialState
    {
        OPEN,
        CLOSE
    };

    explicit I2C(uint8_t bus, uint8_t address, InitialState initialState,
                 int retries) :
        busStr("/dev/i2c-" + std::to_string(bus)),
        device_address(address),
        max_retries(retries)
    {
        if (initialState == InitialState::OPEN)
        {
            open();
        }
    }

    void send_receive(uint8_t addr, uint8_t writeSize, const uint8_t* writeData,
                      uint8_t& readSize, uint8_t* readData) const;

    bool isOpen() const
    {
        return (fd != INVALID_FD);
    }

    void close();

}; // end class I2C

std::unique_ptr<I2C> create(uint8_t bus, uint8_t addr);

} // end namespace i2c
