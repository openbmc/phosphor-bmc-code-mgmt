#include "i2c-vr/tda38640a/tda38640a.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

namespace phosphor::software::VR
{
namespace
{

class TDA38640AImageTest : public ::testing::Test
{
  protected:
    static bool verifyImage(const char* image)
    {
        sdbusplus::async::context ctx;
        TDA38640A device{ctx, 0, 0x40};
        bool accepted = true;
        auto spawnVerify = [&]() -> sdbusplus::async::task<void> {
            accepted = co_await device.verifyImage(
                reinterpret_cast<const uint8_t*>(image), std::strlen(image));
            ctx.request_stop();
        };
        ctx.spawn(spawnVerify());
        ctx.run();
        return accepted;
    }
};

TEST_F(TDA38640AImageTest, RejectsNonHexPartNumberRevision)
{
    static const char image[] = "Part Number : TDA38640Z\n"
                                "[Configuration Data]\n"
                                "[End Configuration Data]\n";

    EXPECT_FALSE(verifyImage(image));
}

TEST_F(TDA38640AImageTest, RejectsEmptyChecksumValue)
{
    static const char image[] =
        "Part Number : TDA38640A\n"
        "Configuration Checksum : 0x\n"
        "[Configuration Data]\n"
        "[End Configuration Data]\n";

    EXPECT_FALSE(verifyImage(image));
}

TEST_F(TDA38640AImageTest, RejectsNonHexDataByte)
{
    static const char image[] =
        "Part Number : TDA38640A\n"
        "[Configuration Data]\n"
        "0040 ZZ 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "[End Configuration Data]\n";

    EXPECT_FALSE(verifyImage(image));
}

} // namespace
} // namespace phosphor::software::VR
