#include "common/include/utils.hpp"

#include <sdbusplus/async.hpp>

#include <functional>
#include <optional>
#include <string>

#include <gtest/gtest.h>

class AsyncSystemTest : public testing::Test
{
  protected:
    auto runCommand(const std::string& cmd,
                    std::optional<std::reference_wrapper<std::string>> output)
        -> bool
    {
        bool success = false;
        ctx.spawn([this, &success, &cmd, output]() -> sdbusplus::async::task<void> {
            success = co_await asyncSystem(ctx, cmd, output);
            ctx.request_stop();
            co_return;
        }());
        ctx.run();
        return success;
    }

    sdbusplus::async::context ctx;
};

TEST_F(AsyncSystemTest, ReturnsTrueOnSuccessWithoutOutput)
{
    EXPECT_TRUE(runCommand("true", std::nullopt));
}

TEST_F(AsyncSystemTest, ReturnsFalseOnFailureWithoutOutput)
{
    EXPECT_FALSE(runCommand("false", std::nullopt));
}

TEST_F(AsyncSystemTest, CapturesOutputOnSuccessWhenRequested)
{
    std::string output;
    EXPECT_TRUE(runCommand("echo test", output));
    EXPECT_EQ(output, "test\n");
}

TEST_F(AsyncSystemTest, CapturesOutputOnFailureWhenRequested)
{
    std::string output;
    EXPECT_FALSE(runCommand("echo fail; exit 7", output));
    EXPECT_EQ(output, "fail\n");
}
