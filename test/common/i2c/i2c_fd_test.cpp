#include "i2c.hpp"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <sdbusplus/async.hpp>

#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace
{
// Mock state, only active while a test has set mockEnabled.
bool mockEnabled = false;
int mockOpenFd = -1;
int mockSlaveRet = 0;
int mockRdwrRet = 0;
int rdwrCalls = 0;
int closeCalls = 0;
int lastClosedFd = -2;

bool isI2cPath(const char* path)
{
    return path != nullptr && std::strncmp(path, "/dev/i2c-", 9) == 0;
}
} // namespace

extern "C" int open(const char* path, int flags, ...)
{
    if (mockEnabled && isI2cPath(path))
    {
        return mockOpenFd;
    }

    using OpenFn = int (*)(const char*, int, ...);
    static OpenFn real = reinterpret_cast<OpenFn>(dlsym(RTLD_NEXT, "open"));
    mode_t mode = 0;
    if ((flags & O_CREAT) != 0)
    {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    return real(path, flags, mode);
}

extern "C" int ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    va_start(args, request);
    void* arg = va_arg(args, void*);
    va_end(args);

    if (mockEnabled)
    {
        if (request == I2C_SLAVE_FORCE)
        {
            return mockSlaveRet;
        }
        if (request == I2C_RDWR)
        {
            rdwrCalls++;
            return mockRdwrRet;
        }
    }

    using IoctlFn = int (*)(int, unsigned long, ...);
    static IoctlFn real =
        reinterpret_cast<IoctlFn>(dlsym(RTLD_NEXT, "ioctl"));
    return real(fd, request, arg);
}

extern "C" int close(int fd)
{
    if (mockEnabled)
    {
        closeCalls++;
        lastClosedFd = fd;
        return 0;
    }

    using CloseFn = int (*)(int);
    static CloseFn real = reinterpret_cast<CloseFn>(dlsym(RTLD_NEXT, "close"));
    return real(fd);
}

namespace phosphor::i2c
{

class I2CFdTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mockEnabled = true;
        mockOpenFd = -1;
        mockSlaveRet = 0;
        mockRdwrRet = 0;
        rdwrCalls = 0;
        closeCalls = 0;
        lastClosedFd = -2;
    }

    void TearDown() override
    {
        mockEnabled = false;
    }

    static bool asyncSendReceive(const I2C& dev, uint8_t* wr, uint8_t wrSize,
                                 uint8_t* rd, uint8_t rdSize)
    {
        sdbusplus::async::context ctx;
        bool result = false;
        ctx.spawn(
            [](sdbusplus::async::context& c, const I2C& d, uint8_t* w,
               uint8_t ws, uint8_t* r, uint8_t rs,
               bool& out) -> sdbusplus::async::task<> {
                out = co_await d.sendReceive(w, ws, r, rs);
                c.request_stop();
            }(ctx, dev, wr, wrSize, rd, rdSize, result));
        ctx.run();
        return result;
    }
};

TEST_F(I2CFdTest, SyncAcceptsDescriptorZero)
{
    mockOpenFd = 0;
    mockRdwrRet = 2;
    I2C dev(1, 0x50);
    ASSERT_TRUE(dev.isOpen());

    std::vector<uint8_t> wr{0x01};
    std::vector<uint8_t> rd(1);
    EXPECT_TRUE(dev.sendReceive(wr, rd));
    EXPECT_EQ(rdwrCalls, 1);
}

TEST_F(I2CFdTest, AsyncAcceptsDescriptorZero)
{
    mockOpenFd = 0;
    mockRdwrRet = 2;
    I2C dev(1, 0x50);
    ASSERT_TRUE(dev.isOpen());

    uint8_t wr = 0x01;
    uint8_t rd = 0;
    EXPECT_TRUE(asyncSendReceive(dev, &wr, 1, &rd, 1));
    EXPECT_EQ(rdwrCalls, 1);
}

TEST_F(I2CFdTest, SyncAcceptsPositiveDescriptor)
{
    mockOpenFd = 5;
    mockRdwrRet = 2;
    I2C dev(1, 0x50);
    ASSERT_TRUE(dev.isOpen());

    std::vector<uint8_t> wr{0x01};
    std::vector<uint8_t> rd(1);
    EXPECT_TRUE(dev.sendReceive(wr, rd));
    EXPECT_EQ(rdwrCalls, 1);
}

TEST_F(I2CFdTest, AsyncAcceptsPositiveDescriptor)
{
    mockOpenFd = 5;
    mockRdwrRet = 2;
    I2C dev(1, 0x50);
    ASSERT_TRUE(dev.isOpen());

    uint8_t wr = 0x01;
    uint8_t rd = 0;
    EXPECT_TRUE(asyncSendReceive(dev, &wr, 1, &rd, 1));
    EXPECT_EQ(rdwrCalls, 1);
}

TEST_F(I2CFdTest, OpenFailureRejectsTransfers)
{
    mockOpenFd = -1;
    I2C dev(1, 0x50);
    EXPECT_FALSE(dev.isOpen());

    std::vector<uint8_t> wr{0x01};
    std::vector<uint8_t> rd(1);
    EXPECT_FALSE(dev.sendReceive(wr, rd));

    uint8_t w = 0x01;
    uint8_t r = 0;
    EXPECT_FALSE(asyncSendReceive(dev, &w, 1, &r, 1));
    EXPECT_EQ(rdwrCalls, 0);
}

TEST_F(I2CFdTest, SlaveSelectFailureClosesDescriptor)
{
    mockOpenFd = 0;
    mockSlaveRet = -1;
    I2C dev(1, 0x50);
    EXPECT_FALSE(dev.isOpen());
    EXPECT_EQ(closeCalls, 1);
    EXPECT_EQ(lastClosedFd, 0);

    std::vector<uint8_t> wr{0x01};
    std::vector<uint8_t> rd(1);
    EXPECT_FALSE(dev.sendReceive(wr, rd));
    EXPECT_EQ(rdwrCalls, 0);
}

TEST_F(I2CFdTest, SyncTransferErrors)
{
    mockOpenFd = 0;
    I2C dev(1, 0x50);
    ASSERT_TRUE(dev.isOpen());

    std::vector<uint8_t> wr{0x01};
    std::vector<uint8_t> rd(1);

    mockRdwrRet = -1;
    EXPECT_FALSE(dev.sendReceive(wr, rd));

    mockRdwrRet = 1;
    EXPECT_FALSE(dev.sendReceive(wr, rd));
    EXPECT_EQ(rdwrCalls, 2);
}

TEST_F(I2CFdTest, AsyncTransferErrors)
{
    mockOpenFd = 0;
    I2C dev(1, 0x50);
    ASSERT_TRUE(dev.isOpen());

    uint8_t wr = 0x01;
    uint8_t rd = 0;

    mockRdwrRet = -1;
    EXPECT_FALSE(asyncSendReceive(dev, &wr, 1, &rd, 1));

    mockRdwrRet = 1;
    EXPECT_FALSE(asyncSendReceive(dev, &wr, 1, &rd, 1));
    EXPECT_EQ(rdwrCalls, 2);
}

} // namespace phosphor::i2c
