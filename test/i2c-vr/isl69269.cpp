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

// Hex image lines are decoded as: <type> <size> <addr> <cmd> [data...] <pec>
//   type: 00 = data record, 49 = header record, anything else is unsupported
//   size: byte count of everything after the size byte (addr + cmd + data +
//         pec)
//   addr: I2C/PMBus address byte
//   cmd:  PMBus command code
//   data: zero or more payload bytes
//   pec:  CRC-8 (polynomial 0x07) over addr, cmd and data
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
    TestISL69269 regulator{ctx, 65535, 0x40,
                           phosphor::software::VR::ISL69269::Gen::Gen2};
};

TEST_F(ISL69269Test, RejectsOddLengthHexLine)
{
    // A single hex digit cannot form a whole byte.
    EXPECT_FALSE(verifyImage("0\n"));
}

TEST_F(ISL69269Test, RejectsInvalidHexDigit)
{
    // "GG" is not valid hexadecimal.
    EXPECT_FALSE(verifyImage("GG\n"));
}

TEST_F(ISL69269Test, RejectsRecordWithMismatchedLength)
{
    // 00 = data record, 05 = size, C2 = addr, E7 = cmd, 08 = data, F6 = pec.
    // The size claims 5 bytes follow, but only 4 are present.
    EXPECT_FALSE(verifyImage("0005C2E708F6\n"));
}

TEST_F(ISL69269Test, RejectsUnsupportedRecordType)
{
    // 01 = unsupported record type (only 00 and 49 are known), 03 = size,
    // C2 = addr, E7 = cmd, F6 = pec. Only the type is bad.
    EXPECT_FALSE(verifyImage("0103C2E7F6\n"));
}

TEST_F(ISL69269Test, RejectsShortHeaderRecord)
{
    // 49 = header record, 03 = size, C2 = addr, AD = cmd (PMBus device ID),
    // F6 = pec. A header record needs a 4-byte payload; this one has none.
    EXPECT_FALSE(verifyImage("4903C2ADF6\n"));
}

TEST_F(ISL69269Test, AcceptsValidGen2Image)
{
    // Line 1: 49 = header record, 07 = size, C2 = addr, 00 = cmd (marks a Gen2
    //         hex file), 00000000 = 4-byte payload, 00 = pec.
    // Line 2: 00 = data record, 05 = size, C2 = addr, E7 = cmd, 0800 = 2-byte
    //         payload, F6 = pec, the CRC-8 of C2 E7 08 00.
    constexpr std::string_view image = "4907C2000000000000\n"
                                       "0005C2E70800F6\n";

    EXPECT_TRUE(regulator.parseAndCheckImage(image));
}

TEST_F(ISL69269Test, AllowsForcedUpdate)
{
    EXPECT_TRUE(regulator.forcedUpdateAllowed());
}

} // namespace
