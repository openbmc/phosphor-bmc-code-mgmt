#include "i2c.hpp"

#include <unistd.h>

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

namespace i2c
{

void I2C::open()
{
    if (isOpen())
    {
        throw I2CException("Device already open", busStr, device_address);
    }

    int retries = 0;
    do
    {
        this->fd = ::open(busStr.c_str(), O_RDWR);
    } while ((fd == -1) && (++retries <= max_retries));

    if (fd == -1)
    {
        throw I2CException("Failed to open", busStr, device_address, errno);
    }

    retries = 0;
    int ret = 0;
    do
    {
        ret = ioctl(fd, I2C_SLAVE_FORCE, device_address);
    } while ((ret < 0) && (++retries <= max_retries));

    if (ret < 0)
    {
        // Close device since setting slave address failed
        closeWithoutException();

        throw I2CException("Failed to set I2C_SLAVE", busStr, device_address,
                           errno);
    }
}

void I2C::send_receive(uint8_t addr, uint8_t writeSize, const uint8_t* writeData,
                       uint8_t& readSize, uint8_t* readData) const
{
    (void)*readData;
    if (!isOpen())
    {
        throw I2CException("fd not open", busStr, device_address);
    }

    uint8_t writebuf[64] = {0};
    uint8_t readbuf[64] = {0};
    struct i2c_msg msg[2];
    struct i2c_rdwr_ioctl_data readWriteData;
    int n_msg = 0;

    if (writeSize)
    {
        writebuf[0] = addr;
        writebuf[1] = writeSize;
        std::memcpy(&(writebuf[2]), writeData, writeSize);

        msg[n_msg].addr = this->device_address;
        msg[n_msg].flags = 0;
        msg[n_msg].len = sizeof(writebuf);
        msg[n_msg].buf = writebuf;
        n_msg++;
    }

    if (readSize)
    {
        msg[n_msg].addr = this->device_address;
        msg[n_msg].flags = I2C_M_RD;
        msg[n_msg].len = sizeof(readbuf);
        msg[n_msg].buf = readbuf;
        n_msg++;
    }

    std::cout << "msg 0 addr: " << msg[0].addr << std::endl;

    readWriteData.msgs = msg;
    readWriteData.nmsgs = n_msg;

    if (ioctl(this->fd, I2C_RDWR, &readWriteData) < 0)
    {
        throw I2CException("ioctl I2C_RDWR failed", busStr, device_address);
    }

    std::cout << "after IOCTL" << std::endl;

    for (size_t i = 0; i < sizeof(readbuf); i++)
    {
        std::cout << "Field: " << i << "" << readbuf[i] << std::endl;
    }
}

void I2C::close()
{
    ::close(this->fd);
}

std::unique_ptr<I2C> create(uint8_t bus, uint8_t addr)
{
    std::unique_ptr<I2C> dev(new I2C(bus, addr, I2C::InitialState::OPEN, 1));
    return dev;
}
} // end namespace i2c
