#include "i2c.hpp"

#include <unistd.h>

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

namespace phosphor::i2c
{

int I2C::open()
{
    int ret = 0;
    fd = ::open(busStr.c_str(), O_RDWR);
    if (fd < 0)
    {
        return fd;
    }

    ret = ioctl(fd, I2C_SLAVE_FORCE, deviceNode);
    if (ret < 0)
    {
        return close();
    }

    return 0;
}

int I2C::sendReceive(uint8_t* writeData, uint8_t writeSize,
                       uint8_t* readData, uint8_t readSize) const
{
    if (fd <= 0)
    {
        return -1;
    }

    struct i2c_msg msg[2];
    struct i2c_rdwr_ioctl_data readWriteData;
    int n_msg = 0;

    if (writeSize)
    {
        msg[n_msg].addr = deviceNode;
        msg[n_msg].flags = 0;
        msg[n_msg].len = writeSize;
        msg[n_msg].buf = writeData;
        n_msg++;
    }

    if (readSize)
    {
        msg[n_msg].addr = deviceNode;
        msg[n_msg].flags = I2C_M_RD;
        msg[n_msg].len = readSize;
        msg[n_msg].buf = readData;
        n_msg++;
    }

    readWriteData.msgs = msg;
    readWriteData.nmsgs = n_msg;

    if (int ret = ioctl(fd, I2C_RDWR, &readWriteData) < 0)
    {
        return ret;
    }
    return 0;
}

int I2C::close()
{
    return ::close(fd);
}

} // end namespace i2c
