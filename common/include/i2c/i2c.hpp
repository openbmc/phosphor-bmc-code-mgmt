#pragma once

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <cstdint>
#include <cstring>
#include <string>

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

namespace phosphor::i2c
{

class I2C
{
  public:
    explicit I2C(uint8_t bus, uint8_t node) :
        busStr("/dev/i2c-" + std::to_string(bus)), deviceNode(node)
    {
        open();
    }

    I2C(I2C& i2c) = delete;
    I2C& operator=(I2C other) = delete;
    I2C(I2C&& other) = delete;
    I2C& operator=(I2C&& other) = delete;

    ~I2C()
    {
        this->close();
    }

    int sendReceive(uint8_t* writeData, uint8_t writeSize, uint8_t* readData,
                    uint8_t readSize) const;

    bool isOpen() const
    {
        return (fd != invalidId);
    }

    int close() const;

  private:
    static constexpr int invalidId = -1;
    int fd = invalidId;
    std::string busStr;
    uint8_t deviceNode;
    int open();
}; // end class I2C

} // namespace phosphor::i2c
