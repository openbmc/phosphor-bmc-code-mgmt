#include "../nopdevice/nopdevice.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>

#include <memory>
#include <regex>

#include <gtest/gtest.h>

// NOLINTBEGIN
sdbusplus::async::task<> endContext(sdbusplus::async::context& ctx)
// NOLINTEND
{
    ctx.request_stop();

    co_return;
}

TEST(SoftwareTest, TestSoftwareConstructor)
{
    sdbusplus::async::context ctx;
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(ctx, cu);

    // the software version is currently unknown
    assert(device->softwareCurrent == nullptr);

    auto sw = std::make_unique<Software>(ctx, *device);

    // since that software is not an update, there is no progress
    assert(sw->optSoftwareActivationProgress == nullptr);

    // test succeeds if we construct without any exception
    // and can run the context

    ctx.spawn(endContext(ctx));
    ctx.run();
}

TEST(SoftwareTest, TestSoftwareId)
{
    sdbusplus::async::context ctx;
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(ctx, cu);

    // the software version is currently unknown
    assert(device->softwareCurrent == nullptr);

    auto sw = std::make_unique<Software>(ctx, *device);

    // design: Swid = <DeviceX>_<RandomId>
    assert(sw->swid.starts_with("HostSPIFlash"));
}

TEST(SoftwareTest, TestSoftwareObjectPath)
{
    sdbusplus::async::context ctx;
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(ctx, cu);

    auto sw = std::make_unique<Software>(ctx, *device);

    lg2::debug("{PATH}", "PATH", sw->getObjectPath());

    // assert that the object path is as per the design
    // design: /xyz/openbmc_project/Software/<SwId>
    assert(std::string(sw->getObjectPath())
               .starts_with("/xyz/openbmc_project/software/"));
}
