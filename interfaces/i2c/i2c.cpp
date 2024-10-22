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

int I2C::open()
{
    int ret;
    this->fd = ::open(busStr.c_str(), O_RDWR);
    if (fd < 0)
    {
        return fd;
    }

    ret = ioctl(fd, I2C_SLAVE_FORCE, device_address);
    if (ret < 0)
    {
        return this->close();
    }

    return 0;
}

int I2C::send_receive(uint8_t* writeData, uint8_t writeSize,
                       uint8_t* readData, uint8_t readSize) const
{
    if (this->fd <= 0)
    {
        return -1;
    }

    struct i2c_msg msg[2];
    struct i2c_rdwr_ioctl_data readWriteData;
    int n_msg = 0;

    if (writeSize)
    {
        msg[n_msg].addr = this->device_address;
        msg[n_msg].flags = 0;
        msg[n_msg].len = writeSize;
        msg[n_msg].buf = writeData;
        n_msg++;
    }

    if (readSize)
    {
        msg[n_msg].addr = this->device_address;
        msg[n_msg].flags = I2C_M_RD;
        msg[n_msg].len = readSize;
        msg[n_msg].buf = readData;
        n_msg++;
    }

    readWriteData.msgs = msg;
    readWriteData.nmsgs = n_msg;

    if (int ret = ioctl(this->fd, I2C_RDWR, &readWriteData) < 0)
    {
        return ret;
    }
    return 0;
}

int I2C::close()
{
    return ::close(this->fd);
}

std::unique_ptr<I2C> create(uint8_t bus, uint8_t addr)
{
    std::unique_ptr<I2C> dev(new I2C(bus, addr));
    return dev;
}
} // end namespace i2c
