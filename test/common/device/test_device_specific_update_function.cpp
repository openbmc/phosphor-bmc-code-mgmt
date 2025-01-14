#include "../nopdevice/nopdevice.hpp"
#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

#include <gtest/gtest.h>

class DeviceTest : public testing::Test
{
    void SetUp() override {}
    void TearDown() override {}
};

// NOLINTBEGIN
sdbusplus::async::task<>
    testDeviceSpecificUpdateFunction(sdbusplus::async::context& ctx)
// NOLINTEND
{
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(ctx, cu);

    // NOLINTBEGIN
    std::unique_ptr<SoftwareActivationProgress> sap =
        std::make_unique<SoftwareActivationProgress>(ctx, "/");
    uint8_t buffer[10];
    size_t buffer_size = 10;
    bool success =
        co_await device->updateDevice((const uint8_t*)buffer, buffer_size, sap);

    assert(success);

    // NOLINTEND

    ctx.request_stop();

    co_return;
}

TEST_F(DeviceTest, TestDeviceConstructor)
{
    sdbusplus::async::context ctx;
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(ctx, cu);

    assert(device->getEMConfigType().starts_with("Nop"));

    // the software version is currently unknown
    assert(device->softwareCurrent == nullptr);
}

TEST_F(DeviceTest, TestDeviceSpecificUpdateFunction)
{
    sdbusplus::async::context ctx;

    // NOLINTBEGIN
    ctx.spawn(testDeviceSpecificUpdateFunction(ctx));
    // NOLINTEND

    ctx.run();
}
