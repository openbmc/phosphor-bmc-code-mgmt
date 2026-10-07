#include "cpld/lattice/lattice_base_cpld.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>

#include <gtest/gtest.h>

using namespace phosphor::software::cpld;

namespace
{

// No /dev/i2c-65535 exists, so every I2C transfer fails deterministically.
constexpr uint16_t testBus = 0xFFFF;
constexpr uint8_t testAddress = 0x40;

class TestLatticeCPLD : public LatticeBaseCPLD
{
  public:
    explicit TestLatticeCPLD(sdbusplus::async::context& ctx) :
        LatticeBaseCPLD(ctx, testBus, testAddress, "LCMXO3LF-4300C", "", false)
    {}

    sdbusplus::async::task<bool> readBusy(uint8_t& busyFlag)
    {
        return readBusyFlag(busyFlag);
    }

  private:
    sdbusplus::async::task<bool> prepareUpdate(const uint8_t* /*unused*/,
                                               size_t /*unused*/) override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> doErase() override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> doUpdate() override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> finishUpdate() override
    {
        co_return false;
    }
    sdbusplus::async::task<bool> readUserCode(uint32_t& /*unused*/) override
    {
        co_return false;
    }
};

sdbusplus::async::task<> readBusyFlagTask(sdbusplus::async::context& ctx,
                                          TestLatticeCPLD& cpld, bool& result,
                                          uint8_t& busyFlag)
{
    result = co_await cpld.readBusy(busyFlag);
    ctx.request_stop();
}

class LatticeBusyFlagTest : public testing::Test
{
  protected:
    LatticeBusyFlagTest() = default;
    ~LatticeBusyFlagTest() noexcept override {}

    sdbusplus::async::context ctx;

  public:
    LatticeBusyFlagTest(const LatticeBusyFlagTest&) = delete;
    LatticeBusyFlagTest(LatticeBusyFlagTest&&) = delete;
    LatticeBusyFlagTest& operator=(const LatticeBusyFlagTest&) = delete;
    LatticeBusyFlagTest& operator=(LatticeBusyFlagTest&&) = delete;
};

} // namespace

TEST_F(LatticeBusyFlagTest, I2CFailureIsReported)
{
    TestLatticeCPLD cpld(ctx);
    bool result = true;
    uint8_t busyFlag = 0x55;

    ctx.spawn(readBusyFlagTask(ctx, cpld, result, busyFlag));
    ctx.run();

    EXPECT_FALSE(result);
    EXPECT_EQ(busyFlag, 0x55);
}
