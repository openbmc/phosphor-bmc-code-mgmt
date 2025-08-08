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

constexpr const char* exampleEndpoint = "/xyz/example_endpoint";

class SoftwareAssocTest : public testing::Test
{
  protected:
    SoftwareAssocTest() :
        exampleUpdater(ctx, true, "swVersion"),
        device(exampleUpdater.getDevice())
    {}
    ~SoftwareAssocTest() noexcept override {}

    sdbusplus::async::context ctx;
    ExampleCodeUpdater exampleUpdater;
    std::unique_ptr<ExampleDevice>& device;

  public:
    SoftwareAssocTest(const SoftwareAssocTest&) = delete;
    SoftwareAssocTest(SoftwareAssocTest&&) = delete;
    SoftwareAssocTest& operator=(const SoftwareAssocTest&) = delete;
    SoftwareAssocTest& operator=(SoftwareAssocTest&&) = delete;
};

sdbusplus::async::task<> testSoftwareAssociationMissing(
    sdbusplus::async::context& ctx, ExampleCodeUpdater& exampleUpdater,
    std::unique_ptr<ExampleDevice>& device)
{
    std::string objPathCurrentSoftware =
        reinterpret_cast<ExampleSoftware*>(device->softwareCurrent.get())
            ->objectPath;

    auto busName = exampleUpdater.getBusName();

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(busName)
            .path(objPathCurrentSoftware);

    // by default there is no association on the software
    try
    {
        co_await client.associations();

        EXPECT_TRUE(false);
    }
    catch (std::exception& e)
    {
        error(e.what());
    }

    ctx.request_stop();

    co_return;
}

TEST_F(SoftwareAssocTest, TestSoftwareAssociationMissing)
{
    ctx.spawn(testSoftwareAssociationMissing(ctx, exampleUpdater, device));
    ctx.run();
}

sdbusplus::async::task<> testSoftwareAssociationRunning(
    sdbusplus::async::context& ctx, ExampleCodeUpdater& exampleUpdater,
    std::unique_ptr<ExampleDevice>& device)
{
    std::string objPathCurrentSoftware =
        reinterpret_cast<ExampleSoftware*>(device->softwareCurrent.get())
            ->objectPath;

    auto busName = exampleUpdater.getBusName();

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(busName)
            .path(objPathCurrentSoftware);

    reinterpret_cast<ExampleSoftware*>(device->softwareCurrent.get())
        ->createInventoryAssociation(true, exampleEndpoint);

    try
    {
        auto res = co_await client.associations();

        EXPECT_EQ(res.size(), 1);

        EXPECT_EQ(std::get<2>(res[0]), exampleEndpoint);
    }
    catch (std::exception& e)
    {
        error(e.what());
        EXPECT_TRUE(false);
    }

    ctx.request_stop();

    co_return;
}

TEST_F(SoftwareAssocTest, TestSoftwareAssociationRunning)
{
    ctx.spawn(testSoftwareAssociationRunning(ctx, exampleUpdater, device));
    ctx.run();
}

sdbusplus::async::task<> testSoftwareAssociationActivating(
    sdbusplus::async::context& ctx, ExampleCodeUpdater& exampleUpdater,
    std::unique_ptr<ExampleDevice>& device)
{
    std::string objPathCurrentSoftware =
        reinterpret_cast<ExampleSoftware*>(device->softwareCurrent.get())
            ->objectPath;

    auto busName = exampleUpdater.getBusName();

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(busName)
            .path(objPathCurrentSoftware);

    // in testing context, we use Example* object, cast is used to make members
    // public
    reinterpret_cast<ExampleSoftware*>(device->softwareCurrent.get())
        ->createInventoryAssociation(false, exampleEndpoint);

    try
    {
        auto res = co_await client.associations();

        EXPECT_EQ(res.size(), 1);

        EXPECT_EQ(std::get<2>(res[0]), exampleEndpoint);
    }
    catch (std::exception& e)
    {
        error(e.what());
        EXPECT_TRUE(false);
    }

    ctx.request_stop();

    co_return;
}

TEST_F(SoftwareAssocTest, TestSoftwareAssociationActivating)
{
    // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(testSoftwareAssociationActivating(ctx, exampleUpdater, device));
    ctx.run();
}
