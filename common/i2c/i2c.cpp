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
        close();
        return ret;
    }

    return 0;
}

int I2C::sendReceive(uint8_t* writeData, uint8_t writeSize, uint8_t* readData,
                     uint8_t readSize) const
{
    if (fd <= 0)
    {
        return -1;
    }

    struct i2c_msg msg[2];
    struct i2c_rdwr_ioctl_data readWriteData;
    int msgIndex = 0;

    if (writeSize)
    {
        msg[msgIndex].addr = deviceNode;
        msg[msgIndex].flags = 0;
        msg[msgIndex].len = writeSize;
        msg[msgIndex].buf = writeData;
        msgIndex++;
    }

    if (readSize)
    {
        msg[msgIndex].addr = deviceNode;
        msg[msgIndex].flags = I2C_M_RD;
        msg[msgIndex].len = readSize;
        msg[msgIndex].buf = readData;
        msgIndex++;
    }

    readWriteData.msgs = msg;
    readWriteData.nmsgs = msgIndex;

    if (int ret = ioctl(fd, I2C_RDWR, &readWriteData) < 0)
    {
        return ret;
    }
    return 0;
}

int I2C::close() const
{
    return ::close(fd);
}

} // namespace phosphor::i2c
