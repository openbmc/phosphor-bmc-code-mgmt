#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

namespace i2c
{

class I2C
{
  private:
    static constexpr int INVALID_FD = -1;
    int fd = INVALID_FD;
    std::string busStr;
    uint8_t device_address;
    int max_retries;
    int open();

  public:
    explicit I2C(uint8_t bus, uint8_t address) :
        busStr("/dev/i2c-" + std::to_string(bus)),
        device_address(address)
    {
        open();
    }

    int send_receive(uint8_t* writeData, uint8_t writeSize,
                      uint8_t* readData, uint8_t readSize) const;

    bool isOpen() const
    {
        return (fd != INVALID_FD);
    }

    int close();

}; // end class I2C

} // end namespace i2c
