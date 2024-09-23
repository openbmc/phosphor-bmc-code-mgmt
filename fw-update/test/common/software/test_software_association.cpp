#include "../nopdevice/nopdevice.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/client.hpp>

#include <memory>

#include <gtest/gtest.h>

class SoftwareTest : public testing::Test
{
    void SetUp() override {}
    void TearDown() override {}
};

// NOLINTBEGIN
sdbusplus::async::task<> endContext(sdbusplus::async::context& ctx)
// NOLINTEND
{
    ctx.request_stop();

    co_return;
}

// NOLINTBEGIN
sdbusplus::async::task<> testSoftwareAssociation(sdbusplus::async::context& ctx)
// NOLINTEND
{
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    std::string service = nopcu.setupBusName();

    std::unique_ptr<NopDevice> device = std::make_unique<NopDevice>(ctx, cu);

    auto sw = std::make_unique<Software>(
        ctx, "/xyz/openbmc_project/inventory/item9", "vUnknown", *device);

    device->softwareCurrent = std::move(sw);

    std::string objPathCurrentSoftware =
        device->softwareCurrent->getObjectPath();

    auto client =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(service)
            .path(objPathCurrentSoftware);

    std::vector<std::tuple<std::string, std::string, std::string>> res =
        co_await client.associations();

    assert(res.size() == 1);

    auto assoc = res[0];

    std::string forward = std::get<0>(assoc);
    std::string reverse = std::get<1>(assoc);
    std::string endpoint = std::get<2>(assoc);

    assert(forward == "inventory");
    assert(reverse == "software");
    assert(endpoint == "/xyz/openbmc_project/inventory/item9");

    ctx.request_stop();

    co_return;
}

TEST_F(SoftwareTest, TestSoftwareAssociation)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testSoftwareAssociation(ctx));

    ctx.run();
}
