#include "../exampledevice/example_device.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Software/Update/client.hpp>
#include <xyz/openbmc_project/Software/Version/client.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;

// NOLINTBEGIN
sdbusplus::async::task<> testSoftwareVersion(sdbusplus::async::context& ctx)
// NOLINTEND
{
    ExampleCodeUpdater nopcu(ctx);
    ExampleCodeUpdater* cu = &nopcu;

    const std::string service =
        "xyz.openbmc_project.Software.NopTestSoftwareVersion";
    ctx.get_bus().request_name(service.c_str());

    auto device = std::make_unique<ExampleDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    std::string objPathCurrentSoftware = device->softwareCurrent->objectPath;

    auto clientVersion =
        sdbusplus::client::xyz::openbmc_project::software::Version<>(ctx)
            .service(service)
            .path(objPathCurrentSoftware);

    // the version is unavailable at this point
    try
    {
        co_await clientVersion.version();
        assert(false);
    }
    catch (std::exception& e)
    {
        debug(e.what());
    }

    // now the version is available
    {
        device->softwareCurrent->setVersion("v12.6");

        assert((co_await clientVersion.version()) == "v12.6");
    }

    // we cannot set the version twice
    {
        device->softwareCurrent->setVersion("v20");

        assert((co_await clientVersion.version()) == "v12.6");
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
