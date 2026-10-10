#include "i2c-vr/xdpe1x2xx/xdpe1x2xx.hpp"

#include <sdbusplus/async.hpp>

#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

namespace phosphor::software::VR
{
namespace
{

class XDPE1X2XXImageTest : public ::testing::Test
{
  public:
    ~XDPE1X2XXImageTest() noexcept override = default;

  protected:
    sdbusplus::async::context ctx;

    bool verifyImage(const char* image)
    {
        XDPE1X2XX device{ctx, 0, 0x40};
        bool accepted = false;
        ctx.spawn([&device, &accepted, image,
                   this]() -> sdbusplus::async::task<void> {
            accepted = co_await device.verifyImage(
                reinterpret_cast<const uint8_t*>(image), std::strlen(image));
            ctx.request_stop();
        }());
        ctx.run();
        return accepted;
    }
};

TEST_F(XDPE1X2XXImageTest, RejectsDataBeforeSectionHeader)
{
    // The first data line after [Configuration Data] carries a non-zero
    // offset, so no section header has been established yet.
    static const char image[] =
        "PMBus Address : 0x40\n"
        "[Configuration Data]\n"
        "00000001 DEADBEEF CAFEBABE 0BADF00D\n"
        "00000002 11111111 22222222 33333333\n"
        "[End Configuration Data]\n";

    EXPECT_FALSE(verifyImage(image));
}

TEST_F(XDPE1X2XXImageTest, ValidImageStillAccepted)
{
    // A well-formed single-section image with matching per-section CRCs and
    // checksum: the first data line starts at offset 0 (establishing the
    // section header), so the data-before-header guard must not fire and the
    // image must pass both parseImage and checkImage.
    static const char image[] =
        "PMBus Address : 0x40\n"
        "Checksum : 0x4E7E895F\n"
        "[Configuration Data]\n"
        "00000000 00000010 AABBCCDD 6420DDA7\n"
        "00000001 EE001122 33445566\n"
        "00000002 778899AA EA5DABB8\n"
        "[End Configuration Data]\n";

    EXPECT_TRUE(verifyImage(image));
}

} // namespace
} // namespace phosphor::software::VR
