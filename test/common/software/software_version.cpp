#include "../exampledevice/example_device.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/client.hpp>
#include <xyz/openbmc_project/Software/Version/client.hpp>

#include <gtest/gtest.h>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::example_device;

sdbusplus::async::task<> testSoftwareVersion(sdbusplus::async::context& ctx)
{
    ExampleCodeUpdater exampleUpdater(ctx, true, nullptr);

    auto& device = exampleUpdater.getDevice();

    device->softwareCurrent = std::make_unique<ExampleSoftware>(ctx, *device);

    std::string objPathCurrentSoftware =
        reinterpret_cast<ExampleSoftware*>(device->softwareCurrent.get())
            ->objectPath;

    auto busName = exampleUpdater.getBusName();

    auto clientVersion =
        sdbusplus::client::xyz::openbmc_project::software::Version<>(ctx)
            .service(busName)
            .path(objPathCurrentSoftware);

    // the version is unavailable at this point
    try
    {
        co_await clientVersion.version();
        EXPECT_TRUE(false);
    }
    catch (std::exception& e)
    {
        debug(e.what());
    }

    // now the version is available
    {
        device->softwareCurrent->setVersion("v12.6");

        EXPECT_EQ((co_await clientVersion.version()), "v12.6");
    }

    // we can set the version twice
    {
        device->softwareCurrent->setVersion("v20");

        EXPECT_EQ((co_await clientVersion.version()), "v20");
    }

    ctx.request_stop();

    co_return;
}

TEST(SoftwareUpdate, TestSoftwareUpdate)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testSoftwareVersion(ctx));

    ctx.run();
}
