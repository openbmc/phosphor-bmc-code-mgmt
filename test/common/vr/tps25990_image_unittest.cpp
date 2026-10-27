#include "i2c-vr/tps25990/tps25990.hpp"

#include <cstdint>
#include <string_view>

#include <gtest/gtest.h>

namespace phosphor::software::VR
{
namespace
{

class TPS25990ImageTest : public testing::Test
{
  protected:
    TPS25990ImageTest() = default;
    ~TPS25990ImageTest() noexcept override = default;
    TPS25990ImageTest(const TPS25990ImageTest&) = delete;
    TPS25990ImageTest& operator=(const TPS25990ImageTest&) = delete;
    TPS25990ImageTest(TPS25990ImageTest&&) = delete;
    TPS25990ImageTest& operator=(TPS25990ImageTest&&) = delete;

    bool verify(std::string_view image)
    {
        bool result = false;
        ctx.spawn(verifyTask(image, result));
        ctx.run();
        return result;
    }

  private:
    sdbusplus::async::task<> verifyTask(std::string_view image, bool& result)
    {
        result = co_await regulator.verifyImage(
            reinterpret_cast<const uint8_t*>(image.data()), image.size());
        ctx.request_stop();
        co_return;
    }

    sdbusplus::async::context ctx;
    TPS25990 regulator{ctx, 65535, 0x40};
};

TEST_F(TPS25990ImageTest, RejectsEmptyImage)
{
    EXPECT_FALSE(verify(""));
}

TEST_F(TPS25990ImageTest, RejectsCommentOnlyImage)
{
    EXPECT_FALSE(verify("/* Configuration image */\n\n"));
}

TEST_F(TPS25990ImageTest, RejectsUnrecognizedText)
{
    EXPECT_FALSE(verify("not a configuration image\n"));
}

TEST_F(TPS25990ImageTest, RejectsChecksumWithoutRegisterData)
{
    EXPECT_FALSE(verify("CHECKSUM = 42\n"));
}

TEST_F(TPS25990ImageTest, AcceptsByteRegisterData)
{
    EXPECT_TRUE(verify("CHECKSUM = 42\n59h = 0x12\n"));
}

TEST_F(TPS25990ImageTest, AcceptsWordRegisterData)
{
    EXPECT_TRUE(verify("CHECKSUM = 42\n59h = 0x1234\n"));
}

} // namespace
} // namespace phosphor::software::VR
