#include "../exampledevice/example_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/client.hpp>

#include <memory>

#include <gtest/gtest.h>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::example_device;

sdbusplus::async::task<> testSoftwareAssociationMissing(
    sdbusplus::async::context& ctx)
{
    ExampleCodeUpdater exampleUpdater(
        ctx, "xyz.openbmc_project.Software.NopTestSoftwareAssociationMissing",
        "swVersion");

    auto& device = exampleUpdater.getDevice();

    std::string objPathCurrentSoftware = device->softwareCurrent->objectPath;

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(exampleUpdater.busName)
            .path(objPathCurrentSoftware);

    // by default there is no association on the software
    try
    {
        co_await client.associations();

        EXPECT_TRUE(false);
    }
    catch (std::exception& e)
    {
        debug(e.what());
    }

    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareAssociationMissing)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testSoftwareAssociationMissing(ctx));

    ctx.run();
}

sdbusplus::async::task<> testSoftwareAssociationRunning(
    sdbusplus::async::context& ctx)
{
    ExampleCodeUpdater exampleUpdater(
        ctx, "xyz.openbmc_project.Software.NopTestSoftwareAssociationRunning",
        "swVersion");

    auto& device = exampleUpdater.getDevice();

    std::string objPathCurrentSoftware = device->softwareCurrent->objectPath;

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(exampleUpdater.busName)
            .path(objPathCurrentSoftware);

    co_await device->softwareCurrent->createInventoryAssociations(true);

    auto res = co_await client.associations();

    EXPECT_TRUE(res.empty());

    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareAssociationRunning)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testSoftwareAssociationRunning(ctx));

    ctx.run();
}

sdbusplus::async::task<> testSoftwareAssociationActivating(
    sdbusplus::async::context& ctx)
{
    ExampleCodeUpdater exampleUpdater(
        ctx,
        "xyz.openbmc_project.Software.NopTestSoftwareAssociationActivating",
        "swVersion");

    auto& device = exampleUpdater.getDevice();

    std::string objPathCurrentSoftware = device->softwareCurrent->objectPath;

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(exampleUpdater.busName)
            .path(objPathCurrentSoftware);

    co_await device->softwareCurrent->createInventoryAssociations(false);

    auto res = co_await client.associations();

    EXPECT_TRUE(res.empty());

    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareAssociationActivating)
{
    sdbusplus::async::context ctx;

    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(testSoftwareAssociationActivating(ctx));

    ctx.run();
}
