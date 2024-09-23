#include "../nopdevice/nopdevice.hpp"

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

// NOLINTBEGIN
sdbusplus::async::task<> testSoftwareVersion(sdbusplus::async::context& ctx)
// NOLINTEND
{
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    const std::string service = nopcu.setupBusName();

    auto device = std::make_unique<NopDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    std::string objPathCurrentSoftware =
        device->softwareCurrent->getObjectPath();

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
        lg2::debug(e.what());
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
