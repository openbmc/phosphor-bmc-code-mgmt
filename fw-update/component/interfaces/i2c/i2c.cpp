#include "i2c.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <format>

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

namespace i2c
{

// Maximum number of data bytes in a block read/block write/block process call
// in SMBus 3.0. The maximum was 32 data bytes in SMBus 2.0 and earlier.
constexpr uint8_t I2C_SMBUS3_BLOCK_MAX = 255;

unsigned long I2CDevice::getFuncs()
{
    // If functionality has not been cached
    if (cachedFuncs == NO_FUNCS)
    {
        // Get functionality from adapter
        int ret = 0, retries = 0;
        do
        {
            ret = ioctl(fd, I2C_FUNCS, &cachedFuncs);
        } while ((ret < 0) && (++retries <= maxRetries));

        if (ret < 0)
        {
            cachedFuncs = NO_FUNCS;
            throw I2CException("Failed to get funcs", busStr, devAddr, errno);
        }
    }

    return cachedFuncs;
}

void I2CDevice::checkReadFuncs(int type)
{
    unsigned long funcs = getFuncs();
    switch (type)
    {
        case I2C_SMBUS_BYTE:
            if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE))
            {
                throw I2CException("Missing SMBUS_READ_BYTE", busStr, devAddr);
            }
            break;
        case I2C_SMBUS_BYTE_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA))
            {
                throw I2CException("Missing SMBUS_READ_BYTE_DATA", busStr,
                                   devAddr);
            }
            break;
        case I2C_SMBUS_WORD_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_READ_WORD_DATA))
            {
                throw I2CException("Missing SMBUS_READ_WORD_DATA", busStr,
                                   devAddr);
            }
            break;
        case I2C_SMBUS_BLOCK_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_READ_BLOCK_DATA))
            {
                throw I2CException("Missing SMBUS_READ_BLOCK_DATA", busStr,
                                   devAddr);
            }
            break;
        case I2C_SMBUS_I2C_BLOCK_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_READ_I2C_BLOCK))
            {
                throw I2CException("Missing I2C_FUNC_SMBUS_READ_I2C_BLOCK",
                                   busStr, devAddr);
            }
            break;
        default:
            fprintf(stderr, "Unexpected read size type: %d\n", type);
            assert(false);
            break;
    }
}

void I2CDevice::checkWriteFuncs(int type)
{
    unsigned long funcs = getFuncs();
    switch (type)
    {
        case I2C_SMBUS_BYTE:
            if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE))
            {
                throw I2CException("Missing SMBUS_WRITE_BYTE", busStr, devAddr);
            }
            break;
        case I2C_SMBUS_BYTE_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
            {
                throw I2CException("Missing SMBUS_WRITE_BYTE_DATA", busStr,
                                   devAddr);
            }
            break;
        case I2C_SMBUS_WORD_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_WRITE_WORD_DATA))
            {
                throw I2CException("Missing SMBUS_WRITE_WORD_DATA", busStr,
                                   devAddr);
            }
            break;
        case I2C_SMBUS_BLOCK_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_WRITE_BLOCK_DATA))
            {
                throw I2CException("Missing SMBUS_WRITE_BLOCK_DATA", busStr,
                                   devAddr);
            }
            break;
        case I2C_SMBUS_I2C_BLOCK_DATA:
            if (!(funcs & I2C_FUNC_SMBUS_WRITE_I2C_BLOCK))
            {
                throw I2CException("Missing I2C_FUNC_SMBUS_WRITE_I2C_BLOCK",
                                   busStr, devAddr);
            }
            break;
        case I2C_SMBUS_PROC_CALL:
            if (!(funcs & I2C_FUNC_SMBUS_PROC_CALL))
            {
                throw I2CException("Missing I2C_FUNC_SMBUS_PROC_CALL", busStr,
                                   devAddr);
            }
            break;
        case I2C_SMBUS_BLOCK_PROC_CALL:
            if (!(funcs & I2C_FUNC_SMBUS_BLOCK_PROC_CALL))
            {
                throw I2CException("Missing I2C_FUNC_SMBUS_BLOCK_PROC_CALL",
                                   busStr, devAddr);
            }
            break;
        default:
            fprintf(stderr, "Unexpected write size type: %d\n", type);
            assert(false);
    }
}

void I2CDevice::processCallSMBus(uint8_t addr, uint8_t writeSize,
                                 const uint8_t* writeData, uint8_t& readSize,
                                 uint8_t* readData)
{
    int ret = 0, retries = 0;
    uint8_t buffer[I2C_SMBUS_BLOCK_MAX];
    do
    {
        // Copy write data to buffer. Buffer is also used by SMBus function to
        // store the data read from the device.
        std::memcpy(buffer, writeData, writeSize);
        ret = i2c_smbus_block_process_call(fd, addr, writeSize, buffer);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to execute block process call", busStr,
                           devAddr, errno);
    }

    readSize = static_cast<uint8_t>(ret);
    std::memcpy(readData, buffer, readSize);
}

void I2CDevice::processCallI2C(uint8_t addr, uint8_t writeSize,
                               const uint8_t* writeData, uint8_t& readSize,
                               uint8_t* readData)
{
    // Buffer for block write. Linux supports SMBus 3.0 max size for block
    // write. Buffer will contain register address, byte count, and data bytes.
    constexpr uint16_t writeBufferSize = I2C_SMBUS3_BLOCK_MAX + 2;
    uint8_t writeBuffer[writeBufferSize];

    // Buffer for block read. Linux supports smaller SMBus 2.0 max size for
    // block read. After ioctl() buffer will contain byte count and data bytes.
    constexpr uint16_t readBufferSize = I2C_SMBUS_BLOCK_MAX + 1;
    uint8_t readBuffer[readBufferSize];

    // i2c_msg and i2c_rdwr_ioctl_data structs required for ioctl()
    constexpr unsigned int numMessages = 2;
    struct i2c_msg messages[numMessages];
    struct i2c_rdwr_ioctl_data readWriteData;
    readWriteData.msgs = messages;
    readWriteData.nmsgs = numMessages;

    int ret = 0, retries = 0;
    do
    {
        // Initialize write buffer with reg addr, byte count, and data bytes
        writeBuffer[0] = addr;
        writeBuffer[1] = writeSize;
        std::memcpy(&(writeBuffer[2]), writeData, writeSize);

        // Initialize first i2c_msg to perform block write
        messages[0].addr = devAddr;
        messages[0].flags = 0;
        messages[0].len = writeSize + 2; // 2 == reg addr + byte count
        messages[0].buf = writeBuffer;

        // Initialize read buffer. Set first byte to number of "extra bytes"
        // that will be read in addition to data bytes. Set to 1 since only
        // extra byte is the byte count.
        readBuffer[0] = 1;

        // Initialize second i2c_msg to perform block read. Linux requires the
        // len field to be set to the buffer size.
        messages[1].addr = devAddr;
        messages[1].flags = I2C_M_RD | I2C_M_RECV_LEN;
        messages[1].len = readBufferSize;
        messages[1].buf = readBuffer;

        // Call ioctl() to send the I2C messages
        ret = ioctl(fd, I2C_RDWR, &readWriteData);
    } while ((ret != numMessages) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to execute I2C block process call", busStr,
                           devAddr, errno);
    }
    else if (ret != numMessages)
    {
        throw I2CException(
            std::format(
                "Failed to execute I2C block process call: {} messages sent",
                ret),
            busStr, devAddr);
    }

    // Read size is in first byte; copy remaining data bytes to readData param
    readSize = readBuffer[0];
    std::memcpy(readData, &(readBuffer[1]), readSize);
}

void I2CDevice::open()
{
    if (isOpen())
    {
        throw I2CException("Device already open", busStr, devAddr);
    }

    int retries = 0;
    do
    {
        fd = ::open(busStr.c_str(), O_RDWR);
    } while ((fd == -1) && (++retries <= maxRetries));

    if (fd == -1)
    {
        throw I2CException("Failed to open", busStr, devAddr, errno);
    }

    retries = 0;
    int ret = 0;
    do
    {
        ret = ioctl(fd, I2C_SLAVE, devAddr);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        // Close device since setting slave address failed
        closeWithoutException();

        throw I2CException("Failed to set I2C_SLAVE", busStr, devAddr, errno);
    }
}

void I2CDevice::close()
{
    checkIsOpen();

    int ret = 0, retries = 0;
    do
    {
        ret = ::close(fd);
    } while ((ret == -1) && (++retries <= maxRetries));

    if (ret == -1)
    {
        throw I2CException("Failed to close", busStr, devAddr, errno);
    }

    fd = INVALID_FD;
    cachedFuncs = NO_FUNCS;
}

void I2CDevice::read(uint8_t& data)
{
    checkIsOpen();
    checkReadFuncs(I2C_SMBUS_BYTE);

    int ret = 0, retries = 0;
    do
    {
        ret = i2c_smbus_read_byte(fd);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to read byte", busStr, devAddr, errno);
    }

    data = static_cast<uint8_t>(ret);
}

void I2CDevice::read(uint8_t addr, uint8_t& data)
{
    checkIsOpen();
    checkReadFuncs(I2C_SMBUS_BYTE_DATA);

    int ret = 0, retries = 0;
    do
    {
        ret = i2c_smbus_read_byte_data(fd, addr);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to read byte data", busStr, devAddr, errno);
    }

    data = static_cast<uint8_t>(ret);
}

void I2CDevice::read(uint8_t addr, uint16_t& data)
{
    checkIsOpen();
    checkReadFuncs(I2C_SMBUS_WORD_DATA);

    int ret = 0, retries = 0;
    do
    {
        ret = i2c_smbus_read_word_data(fd, addr);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to read word data", busStr, devAddr, errno);
    }

    data = static_cast<uint16_t>(ret);
}

void I2CDevice::read(uint8_t addr, uint8_t& size, uint8_t* data, Mode mode)
{
    checkIsOpen();

    int ret = -1, retries = 0;
    switch (mode)
    {
        case Mode::SMBUS:
            checkReadFuncs(I2C_SMBUS_BLOCK_DATA);
            do
            {
                ret = i2c_smbus_read_block_data(fd, addr, data);
            } while ((ret < 0) && (++retries <= maxRetries));
            break;
        case Mode::I2C:
            checkReadFuncs(I2C_SMBUS_I2C_BLOCK_DATA);
            do
            {
                ret = i2c_smbus_read_i2c_block_data(fd, addr, size, data);
            } while ((ret < 0) && (++retries <= maxRetries));
            if (ret != size)
            {
                throw I2CException("Failed to read i2c block data", busStr,
                                   devAddr, errno);
            }
            break;
    }

    if (ret < 0)
    {
        throw I2CException("Failed to read block data", busStr, devAddr, errno);
    }

    size = static_cast<uint8_t>(ret);
}

void I2CDevice::write(uint8_t data)
{
    checkIsOpen();
    checkWriteFuncs(I2C_SMBUS_BYTE);

    int ret = 0, retries = 0;
    do
    {
        ret = i2c_smbus_write_byte(fd, data);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to write byte", busStr, devAddr, errno);
    }
}

void I2CDevice::write(uint8_t addr, uint8_t data)
{
    checkIsOpen();
    checkWriteFuncs(I2C_SMBUS_BYTE_DATA);

    int ret = 0, retries = 0;
    do
    {
        ret = i2c_smbus_write_byte_data(fd, addr, data);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to write byte data", busStr, devAddr, errno);
    }
}

void I2CDevice::write(uint8_t addr, uint16_t data)
{
    checkIsOpen();
    checkWriteFuncs(I2C_SMBUS_WORD_DATA);

    int ret = 0, retries = 0;
    do
    {
        ret = i2c_smbus_write_word_data(fd, addr, data);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to write word data", busStr, devAddr, errno);
    }
}

void I2CDevice::write(uint8_t addr, uint8_t size, const uint8_t* data,
                      Mode mode)
{
    checkIsOpen();

    int ret = -1, retries = 0;
    switch (mode)
    {
        case Mode::SMBUS:
            checkWriteFuncs(I2C_SMBUS_BLOCK_DATA);
            do
            {
                ret = i2c_smbus_write_block_data(fd, addr, size, data);
            } while ((ret < 0) && (++retries <= maxRetries));
            break;
        case Mode::I2C:
            checkWriteFuncs(I2C_SMBUS_I2C_BLOCK_DATA);
            do
            {
                ret = i2c_smbus_write_i2c_block_data(fd, addr, size, data);
            } while ((ret < 0) && (++retries <= maxRetries));
            break;
    }

    if (ret < 0)
    {
        throw I2CException("Failed to write block data", busStr, devAddr,
                           errno);
    }
}

void I2CDevice::processCall(uint8_t addr, uint16_t writeData,
                            uint16_t& readData)
{
    checkIsOpen();
    checkWriteFuncs(I2C_SMBUS_PROC_CALL);

    int ret = 0, retries = 0;
    do
    {
        ret = i2c_smbus_process_call(fd, addr, writeData);
    } while ((ret < 0) && (++retries <= maxRetries));

    if (ret < 0)
    {
        throw I2CException("Failed to execute process call", busStr, devAddr,
                           errno);
    }

    readData = static_cast<uint16_t>(ret);
}

void I2CDevice::processCall(uint8_t addr, uint8_t writeSize,
                            const uint8_t* writeData, uint8_t& readSize,
                            uint8_t* readData)
{
    checkIsOpen();
    unsigned long funcs = getFuncs();

    if ((funcs & I2C_FUNC_SMBUS_BLOCK_PROC_CALL) &&
        (writeSize <= I2C_SMBUS_BLOCK_MAX))
    {
        // Use standard SMBus function which supports smaller SMBus 2.0 maximum
        processCallSMBus(addr, writeSize, writeData, readSize, readData);
    }
    else if (funcs & I2C_FUNC_I2C)
    {
        // Use lower level I2C ioctl which supports larger SMBus 3.0 maximum
        processCallI2C(addr, writeSize, writeData, readSize, readData);
    }
    else
    {
        throw I2CException(
            std::format(
                "Block process call unsupported: writeSize={:d}, funcs={}",
                writeSize, funcs),
            busStr, devAddr);
    }
}

std::unique_ptr<I2CInterface> I2CDevice::create(
    uint8_t busId, uint8_t devAddr, InitialState initialState, int maxRetries)
{
    std::unique_ptr<I2CDevice> dev(
        new I2CDevice(busId, devAddr, initialState, maxRetries));
    return dev;
}

std::unique_ptr<I2CInterface>
    create(uint8_t busId, uint8_t devAddr,
           I2CInterface::InitialState initialState, int maxRetries)
{
    return I2CDevice::create(busId, devAddr, initialState, maxRetries);
}

} // namespace i2c
