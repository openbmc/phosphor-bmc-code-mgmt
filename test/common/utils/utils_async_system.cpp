#include "common/include/utils.hpp"

#include <sdbusplus/async.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

class AsyncSystemTest : public testing::Test
{
  protected:
    AsyncSystemTest() = default;
    ~AsyncSystemTest() noexcept override = default;

    auto runCommandTask(
        bool& success, std::string cmd,
        std::optional<std::reference_wrapper<std::string>> output)
        -> sdbusplus::async::task<void>
    {
        success = co_await asyncSystem(ctx, cmd, output);
        ctx.request_stop();
        co_return;
    }

    auto runCommand(const std::string& cmd,
                    std::optional<std::reference_wrapper<std::string>> output)
        -> bool
    {
        bool success = false;
        ctx.spawn(runCommandTask(success, cmd, output));
        ctx.run();
        return success;
    }

    auto runCommandWithCallback(const std::string& cmd,
                                AsyncSystemOutputCallback callback) -> bool
    {
        bool success = false;
        auto task = [&]() -> sdbusplus::async::task<void> {
            success = co_await asyncSystem(ctx, cmd, std::nullopt, callback);
            ctx.request_stop();
            co_return;
        };
        ctx.spawn(task());
        ctx.run();
        return success;
    }

    sdbusplus::async::context ctx;

  public:
    AsyncSystemTest(const AsyncSystemTest&) = delete;
    AsyncSystemTest(AsyncSystemTest&&) = delete;
    AsyncSystemTest& operator=(const AsyncSystemTest&) = delete;
    AsyncSystemTest& operator=(AsyncSystemTest&&) = delete;
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

TEST_F(AsyncSystemTest, DeliversOutputToCallbackWithoutResultString)
{
    std::string received;
    EXPECT_TRUE(
        runCommandWithCallback("echo progress", [&](std::string_view chunk) {
            received.append(chunk);
        }));
    EXPECT_EQ(received, "progress\n");
}
