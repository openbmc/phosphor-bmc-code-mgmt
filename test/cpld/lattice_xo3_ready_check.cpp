#include "cpld/lattice/lattice_xo3_cpld.hpp"

#include <gtest/gtest.h>

namespace phosphor::software::cpld
{
namespace
{

class TestLatticeXO3CPLD : public LatticeXO3CPLD
{
  public:
    explicit TestLatticeXO3CPLD(sdbusplus::async::context& ctx) :
        LatticeXO3CPLD(ctx, 65535, 0x40, "LCMXO3LF_4300C", "CFG0", false)
    {}

    bool runErase(sdbusplus::async::context& ctx)
    {
        bool result = true;
        ctx.spawn(runEraseTask(ctx, result));
        ctx.run();
        return result;
    }

    bool programModeCalled = false;

  protected:
    sdbusplus::async::task<bool> waitBusyAndVerify() override
    {
        co_return false;
    }

    sdbusplus::async::task<bool> enableProgramMode() override
    {
        programModeCalled = true;
        co_return true;
    }

  private:
    sdbusplus::async::task<> runEraseTask(sdbusplus::async::context& ctx,
                                          bool& result)
    {
        result = co_await doErase();
        ctx.request_stop();
        co_return;
    }
};

class LatticeXO3ReadyCheckTest : public testing::Test
{
  protected:
    LatticeXO3ReadyCheckTest() = default;
    ~LatticeXO3ReadyCheckTest() noexcept override = default;
    LatticeXO3ReadyCheckTest(const LatticeXO3ReadyCheckTest&) = delete;
    LatticeXO3ReadyCheckTest& operator=(const LatticeXO3ReadyCheckTest&) =
        delete;
    LatticeXO3ReadyCheckTest(LatticeXO3ReadyCheckTest&&) = delete;
    LatticeXO3ReadyCheckTest& operator=(LatticeXO3ReadyCheckTest&&) = delete;

    sdbusplus::async::context ctx;
    TestLatticeXO3CPLD cpld{ctx};
};

TEST_F(LatticeXO3ReadyCheckTest, EraseStopsWhenDeviceIsNotReady)
{
    EXPECT_FALSE(cpld.runErase(ctx));
    EXPECT_FALSE(cpld.programModeCalled);
}

} // namespace
} // namespace phosphor::software::cpld
