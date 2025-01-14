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
using namespace phosphor::software::example_device;

sdbusplus::async::task<> endContext(sdbusplus::async::context& ctx)
{
    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareConstructor)
{
    sdbusplus::async::context ctx;
    ExampleCodeUpdater exampleUpdater(ctx);

    auto device = std::make_unique<ExampleDevice>(ctx, &exampleUpdater);

    // the software version is currently unknown
    EXPECT_EQ(device->softwareCurrent, nullptr);

    auto sw = std::make_unique<Software>(ctx, *device);

    // since that software is not an update, there is no progress
    EXPECT_EQ(sw->softwareActivationProgress, nullptr);

    // test succeeds if we construct without any exception
    // and can run the context

    ctx.spawn(endContext(ctx));
    ctx.run();
}

TEST(SoftwareTest, TestSoftwareId)
{
    sdbusplus::async::context ctx;
    ExampleCodeUpdater exampleUpdater(ctx);

    auto device = std::make_unique<ExampleDevice>(ctx, &exampleUpdater);

    // the software version is currently unknown
    EXPECT_EQ(device->softwareCurrent, nullptr);

    auto sw = std::make_unique<Software>(ctx, *device);

    // design: Swid = <DeviceX>_<RandomId>
    EXPECT_TRUE(sw->swid.starts_with("ExampleSoftware"));
}

TEST(SoftwareTest, TestSoftwareObjectPath)
{
    sdbusplus::async::context ctx;
    ExampleCodeUpdater exampleUpdater(ctx);

    auto device = std::make_unique<ExampleDevice>(ctx, &exampleUpdater);

    auto sw = std::make_unique<Software>(ctx, *device);

    debug("{PATH}", "PATH", sw->objectPath);

    // assert that the object path is as per the design
    // design: /xyz/openbmc_project/Software/<SwId>
    EXPECT_TRUE(std::string(sw->objectPath)
                    .starts_with("/xyz/openbmc_project/software/"));
}
