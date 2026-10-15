#include "common/include/i2c/i2c.hpp"

#include <cstdarg>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace
{

constexpr auto testPath = "/dev/i2c-65535";

struct SyscallState
{
    int descriptor = 0;
    int initializationResult = 0;
    int transferResult = 2;
    int initializationCalls = 0;
    int transferCalls = 0;
    int closeCalls = 0;
};

SyscallState* activeState = nullptr;

} // namespace

extern "C"
{
int __real_open(const char* path, int flags, ...);
int __real_ioctl(int descriptor, unsigned long request, ...);
int __real_close(int descriptor);

int __wrap_open(const char* path, int flags, ...)
{
    if (activeState != nullptr && std::strcmp(path, testPath) == 0)
    {
        return activeState->descriptor;
    }

    if ((flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE)
    {
        va_list arguments;
        va_start(arguments, flags);
        auto mode = va_arg(arguments, mode_t);
        va_end(arguments);
        return __real_open(path, flags, mode);
    }
    return __real_open(path, flags);
}

#ifdef HAVE_WRAP_OPEN64
int __real_open64(const char* path, int flags, ...);

int __wrap_open64(const char* path, int flags, ...)
{
    if (activeState != nullptr && std::strcmp(path, testPath) == 0)
    {
        return activeState->descriptor;
    }

    if ((flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE)
    {
        va_list arguments;
        va_start(arguments, flags);
        auto mode = va_arg(arguments, mode_t);
        va_end(arguments);
        return __real_open64(path, flags, mode);
    }
    return __real_open64(path, flags);
}
#endif

int __wrap_ioctl(int descriptor, unsigned long request, ...)
{
    if (activeState != nullptr && descriptor == activeState->descriptor)
    {
        if (request == I2C_SLAVE_FORCE)
        {
            ++activeState->initializationCalls;
            return activeState->initializationResult;
        }
        if (request == I2C_RDWR)
        {
            ++activeState->transferCalls;
            return activeState->transferResult;
        }
    }

    va_list arguments;
    va_start(arguments, request);
    auto argument = va_arg(arguments, void*);
    va_end(arguments);
    return __real_ioctl(descriptor, request, argument);
}

#ifdef HAVE_WRAP___IOCTL_TIME64
int __real___ioctl_time64(int descriptor, unsigned long request, ...);

int __wrap___ioctl_time64(int descriptor, unsigned long request, ...)
{
    if (activeState != nullptr && descriptor == activeState->descriptor)
    {
        if (request == I2C_SLAVE_FORCE)
        {
            ++activeState->initializationCalls;
            return activeState->initializationResult;
        }
        if (request == I2C_RDWR)
        {
            ++activeState->transferCalls;
            return activeState->transferResult;
        }
    }

    va_list arguments;
    va_start(arguments, request);
    auto argument = va_arg(arguments, void*);
    va_end(arguments);
    return __real___ioctl_time64(descriptor, request, argument);
}
#endif

int __wrap_close(int descriptor)
{
    if (activeState != nullptr && descriptor == activeState->descriptor)
    {
        ++activeState->closeCalls;
        return 0;
    }
    return __real_close(descriptor);
}
}

namespace
{

class I2CFdTest : public testing::TestWithParam<bool>
{
  protected:
    ~I2CFdTest() noexcept override = default;

    void SetUp() override
    {
        activeState = &state;
    }

    void TearDown() override
    {
        activeState = nullptr;
    }

    sdbusplus::async::task<> transferAsync(phosphor::i2c::I2C& device,
                                           bool& result)
    {
        uint8_t command = 0;
        uint8_t response = 0;
        result = co_await device.sendReceive(&command, 1, &response, 1);
        ctx.request_stop();
    }

    bool transfer(phosphor::i2c::I2C& device)
    {
        if (GetParam())
        {
            bool result = false;
            ctx.spawn(transferAsync(device, result));
            ctx.run();
            return result;
        }
        std::vector<uint8_t> command{0};
        std::vector<uint8_t> response(1);
        return device.sendReceive(command, response);
    }

    sdbusplus::async::context ctx;
    SyscallState state;
};

TEST_P(I2CFdTest, AcceptsDescriptorZero)
{
    {
        phosphor::i2c::I2C device(65535, 0x40);
        EXPECT_TRUE(device.isOpen());
        EXPECT_TRUE(transfer(device));
        EXPECT_EQ(state.initializationCalls, 1);
        EXPECT_EQ(state.transferCalls, 1);
    }
    EXPECT_EQ(state.closeCalls, 1);
}

TEST_P(I2CFdTest, AcceptsPositiveDescriptor)
{
    state.descriptor = 12345;
    phosphor::i2c::I2C device(65535, 0x40);
    EXPECT_TRUE(transfer(device));
    EXPECT_EQ(state.transferCalls, 1);
}

TEST_P(I2CFdTest, RejectsFailedOpen)
{
    state.descriptor = -1;
    phosphor::i2c::I2C device(65535, 0x40);
    EXPECT_FALSE(device.isOpen());
    EXPECT_FALSE(transfer(device));
    EXPECT_EQ(state.initializationCalls, 0);
    EXPECT_EQ(state.transferCalls, 0);
    EXPECT_EQ(state.closeCalls, 0);
}

TEST_P(I2CFdTest, RejectsFailedInitialization)
{
    state.initializationResult = -1;
    {
        phosphor::i2c::I2C device(65535, 0x40);
        EXPECT_FALSE(device.isOpen());
        EXPECT_FALSE(transfer(device));
        EXPECT_EQ(state.transferCalls, 0);
        EXPECT_EQ(state.closeCalls, 1);
    }
    EXPECT_EQ(state.closeCalls, 1);
}

TEST_P(I2CFdTest, RejectsTransferError)
{
    state.transferResult = -1;
    phosphor::i2c::I2C device(65535, 0x40);
    EXPECT_FALSE(transfer(device));
    EXPECT_EQ(state.transferCalls, 1);
}

TEST_P(I2CFdTest, RejectsIncompleteTransfer)
{
    state.transferResult = 1;
    phosphor::i2c::I2C device(65535, 0x40);
    EXPECT_FALSE(transfer(device));
    EXPECT_EQ(state.transferCalls, 1);
}

INSTANTIATE_TEST_SUITE_P(TransferOverloads, I2CFdTest,
                         testing::Values(false, true));

} // namespace
