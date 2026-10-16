#include "i2c-vr/isl69269/isl69269.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

class TestISL69269 : public phosphor::software::VR::ISL69269
{
  public:
    using ISL69269::ISL69269;

    bool parseAndCheckImage(std::string_view image)
    {
        return parseImage(reinterpret_cast<const uint8_t*>(image.data()),
                          image.size()) &&
               checkImage();
    }
};

class ISL69269Test : public testing::Test
{
  protected:
    ISL69269Test() = default;
    ~ISL69269Test() noexcept override = default;
    ISL69269Test(const ISL69269Test&) = delete;
    ISL69269Test& operator=(const ISL69269Test&) = delete;
    ISL69269Test(ISL69269Test&&) = delete;
    ISL69269Test& operator=(ISL69269Test&&) = delete;

    auto verifyImage(std::string_view image) -> bool
    {
        bool result = true;
        ctx.spawn(verifyImageTask(
            result, reinterpret_cast<const uint8_t*>(image.data()),
            image.size()));
        ctx.run();
        return result;
    }

    auto verifyImageTask(bool& result, const uint8_t* image,
                         std::size_t imageSize) -> sdbusplus::async::task<>
    {
        result = co_await regulator.verifyImage(image, imageSize);
        ctx.request_stop();
    }

    sdbusplus::async::context ctx;
    TestISL69269 regulator{
        ctx, 65535, 0x40, phosphor::software::VR::ISL69269::Gen::Gen2};
};

TEST_F(ISL69269Test, RejectsOddLengthHexLine)
{
    EXPECT_FALSE(verifyImage("0\n"));
}

TEST_F(ISL69269Test, RejectsInvalidHexDigit)
{
    EXPECT_FALSE(verifyImage("GG\n"));
}

TEST_F(ISL69269Test, RejectsRecordWithMismatchedLength)
{
    EXPECT_FALSE(verifyImage("0005C2E708F6\n"));
}

TEST_F(ISL69269Test, RejectsUnsupportedRecordType)
{
    EXPECT_FALSE(verifyImage("0103C2E7F6\n"));
}

TEST_F(ISL69269Test, RejectsShortHeaderRecord)
{
    EXPECT_FALSE(verifyImage("4903C2ADF6\n"));
}

TEST_F(ISL69269Test, AcceptsValidGen2Image)
{
    constexpr std::string_view image = "4907C2000000000000\n"
                                       "0005C2E70800F6\n";

    EXPECT_TRUE(regulator.parseAndCheckImage(image));
}

TEST_F(ISL69269Test, AllowsForcedUpdate)
{
    EXPECT_TRUE(regulator.forcedUpdateAllowed());
}

} // namespace
