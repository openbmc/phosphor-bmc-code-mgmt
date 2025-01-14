#include "../exampledevice/example_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

#include <memory>
#include <regex>

#include <gtest/gtest.h>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;

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
    ExampleCodeUpdater nopcu(ctx);
    ExampleCodeUpdater* cu = &nopcu;

    std::string objPath =
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

    SoftwareConfig config(objPath, 0x1234, "my.example.compatible", "Nop",
                          "MB1NopComponent");

    auto device = std::make_unique<ExampleDevice>(ctx, cu, config);

    std::string swid = WrapSoftware::wrapGetRandomSoftwareId(*device);
    debug("{SWID}", "SWID", swid);

    std::regex re("[a-zA-Z0-9]+_[0-9]+");
    std::cmatch m;

    EXPECT_TRUE(std::regex_match(swid.c_str(), m, re));

    EXPECT_TRUE(swid.starts_with("MB1NopComponent_"));
}
