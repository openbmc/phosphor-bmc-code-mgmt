#include "../nopdevice/nopdevice.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

#include <memory>
#include <regex>

#include <gtest/gtest.h>

class WrapSoftware : public Software
{
  public:
    static std::string wrapGetRandomSoftwareId(Device& parent)
    {
        return Software::getRandomSoftwareId(parent);
    };
};

TEST(SoftwareTest, TestSoftwareGetRandomSoftwareId)
{
    sdbusplus::async::context ctx;
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    DeviceConfig config(0x1234, "my.example.compatible", "Nop",
                        "MB1NopComponent");

    auto device = std::make_unique<NopDevice>(ctx, config, cu);

    std::string swid = WrapSoftware::wrapGetRandomSoftwareId(*device);
    lg2::debug("{SWID}", "SWID", swid);

    std::regex re("[a-zA-Z0-9]+_[a-zA-Z0-9]+_[0-9]+");
    std::cmatch m;

    EXPECT_TRUE(std::regex_match(swid.c_str(), m, re));

    EXPECT_TRUE(swid.starts_with("Nop_MB1NopComponent_"));
}
