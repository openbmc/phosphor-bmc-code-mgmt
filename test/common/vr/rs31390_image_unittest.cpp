#include "i2c-vr/tps25990/rs31390.hpp"

#include <cstdint>
#include <string_view>

#include <gtest/gtest.h>

namespace phosphor::software::VR
{
namespace
{

class RS31390ImageTest : public testing::Test
{
  protected:
    RS31390ImageTest() = default;
    ~RS31390ImageTest() noexcept override = default;
    RS31390ImageTest(const RS31390ImageTest&) = delete;
    RS31390ImageTest& operator=(const RS31390ImageTest&) = delete;
    RS31390ImageTest(RS31390ImageTest&&) = delete;
    RS31390ImageTest& operator=(RS31390ImageTest&&) = delete;

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
    RS31390 regulator{ctx, 65535, 0x40};
};

TEST_F(RS31390ImageTest, RejectsSingleColumn)
{
    EXPECT_FALSE(verify("1\n"));
}

TEST_F(RS31390ImageTest, RejectsTwoColumns)
{
    EXPECT_FALSE(verify("1,register\n"));
}

TEST_F(RS31390ImageTest, RejectsThreeColumns)
{
    EXPECT_FALSE(verify("1,register,59\n"));
}

TEST_F(RS31390ImageTest, RejectsFourColumns)
{
    EXPECT_FALSE(verify("1,register,59,unused\n"));
}

TEST_F(RS31390ImageTest, RejectsFiveColumns)
{
    EXPECT_FALSE(verify("1,register,59,unused,unused\n"));
}

TEST_F(RS31390ImageTest, RejectsSixColumns)
{
    EXPECT_FALSE(verify("1,register,59,unused,unused,1\n"));
}

TEST_F(RS31390ImageTest, RejectsMissingTrailingValue)
{
    EXPECT_FALSE(verify("1,register,59,unused,unused,1,\n"));
}

TEST_F(RS31390ImageTest, RejectsTruncatedChecksumRow)
{
    EXPECT_FALSE(verify("1,checksum,B5\n"));
}

TEST_F(RS31390ImageTest, AcceptsByteRegisterData)
{
    EXPECT_TRUE(verify("1,register,59,unused,unused,1,12\n"));
}

TEST_F(RS31390ImageTest, AcceptsWordRegisterData)
{
    EXPECT_TRUE(verify("1,register,59,unused,unused,2,1234\n"));
}

TEST_F(RS31390ImageTest, AcceptsChecksumAndRegisterData)
{
    EXPECT_TRUE(verify("S.No,Name,Command,Unused,Unused,Size,Value\n"
                       "1,checksum,B5,unused,unused,2,1234\n"
                       "2,register,59,unused,unused,1,12\n"));
}

} // namespace
} // namespace phosphor::software::VR
