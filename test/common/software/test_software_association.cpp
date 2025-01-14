#include "../exampledevice/example_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/client.hpp>

#include <memory>

#include <gtest/gtest.h>

// const std::string objPathInventory = "/xyz/openbmc_project/inventory/item9";

// NOLINTBEGIN
sdbusplus::async::task<> testSoftwareAssociationMissing(
    sdbusplus::async::context& ctx)
// NOLINTEND
{
    ExampleCodeUpdater nopcu(ctx);
    ExampleCodeUpdater* cu = &nopcu;

    std::string service = nopcu.setupBusName();

    std::unique_ptr<ExampleDevice> device =
        std::make_unique<ExampleDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    std::string objPathCurrentSoftware =
        device->softwareCurrent->getObjectPath();

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(service)
            .path(objPathCurrentSoftware);

    // by default there is no association on the software
    try
    {
        co_await client.associations();

        assert(false);
    }
    catch (std::exception& e)
    {
        lg2::debug(e.what());
    }

    co_await device->softwareCurrent
        ->setAssociationDefinitionsRunningActivating(false, false);

    assert((co_await client.associations()).empty());

    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareAssociationMissing)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testSoftwareAssociationMissing(ctx));

    ctx.run();
}

// NOLINTBEGIN
sdbusplus::async::task<> testSoftwareAssociationRunning(
    sdbusplus::async::context& ctx)
// NOLINTEND
{
    ExampleCodeUpdater nopcu(ctx);
    ExampleCodeUpdater* cu = &nopcu;

    std::string service = nopcu.setupBusName();

    std::unique_ptr<ExampleDevice> device =
        std::make_unique<ExampleDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    std::string objPathCurrentSoftware =
        device->softwareCurrent->getObjectPath();

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(service)
            .path(objPathCurrentSoftware);

    co_await device->softwareCurrent
        ->setAssociationDefinitionsRunningActivating(true, false);

    auto res = co_await client.associations();

    assert(res.size() == 1);

    auto assoc = res[0];

    std::string forward = std::get<0>(assoc);
    std::string reverse = std::get<1>(assoc);
    std::string endpoint = std::get<2>(assoc);

    assert(forward == "running");
    assert(reverse == "ran_on");
    assert(endpoint == (co_await device->getInventoryItemObjectPath()));

    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareAssociationRunning)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testSoftwareAssociationRunning(ctx));

    ctx.run();
}

// NOLINTBEGIN
sdbusplus::async::task<> testSoftwareAssociationActivating(
    sdbusplus::async::context& ctx)
// NOLINTEND
{
    ExampleCodeUpdater nopcu(ctx);
    ExampleCodeUpdater* cu = &nopcu;

    std::string service = nopcu.setupBusName();

    std::unique_ptr<ExampleDevice> device =
        std::make_unique<ExampleDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    std::string objPathCurrentSoftware =
        device->softwareCurrent->getObjectPath();

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(service)
            .path(objPathCurrentSoftware);

    co_await device->softwareCurrent
        ->setAssociationDefinitionsRunningActivating(false, true);

    auto res = co_await client.associations();

    assert(res.size() == 1);

    auto assoc = res[0];

    std::string forward = std::get<0>(assoc);
    std::string reverse = std::get<1>(assoc);
    std::string endpoint = std::get<2>(assoc);

    assert(forward == "activating");
    assert(reverse == "activated_on");
    assert(endpoint == (co_await device->getInventoryItemObjectPath()));

    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareAssociationActivating)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testSoftwareAssociationActivating(ctx));

    ctx.run();
}
