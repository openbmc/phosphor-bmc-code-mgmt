#include "../exampledevice/example_device.hpp"
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

using namespace phosphor::software::example_device;
using namespace phosphor::software;

sdbusplus::async::task<> testDeviceSpecificUpdateFunction(
    sdbusplus::async::context& ctx)
{
    ExampleCodeUpdater exampleUpdater(ctx,
                                      "xyz.openbmc_project.DeviceSpecefic");

    const auto& device = exampleUpdater.getDevice();

    uint8_t buffer[10];
    size_t buffer_size = 10;
    bool success =
        co_await device->updateDevice((const uint8_t*)buffer, buffer_size);

    EXPECT_TRUE(success);

    ctx.request_stop();

    co_return;
}

TEST(DeviceTest, TestDeviceConstructor)
{
    sdbusplus::async::context ctx;
    ExampleCodeUpdater exampleUpdater(ctx,
                                      "xyz.openbmc_project.UpdaterConstructor");

    auto& device = exampleUpdater.getDevice();

    EXPECT_TRUE(device->getEMConfigType().starts_with("Nop"));

    // the software version is currently unknown
    EXPECT_EQ(device->softwareCurrent, nullptr);
}

TEST(DeviceTest, TestDeviceSpecificUpdateFunction)
{
    sdbusplus::async::context ctx;

    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(testDeviceSpecificUpdateFunction(ctx));
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)

    ctx.run();
}
