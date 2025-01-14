#include "../exampledevice/example_device.hpp"
#include "common/include/device.hpp"
#include "common/include/software_manager.hpp"
#include "test/create_package/create_pldm_fw_package.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

#include <gtest/gtest.h>

using namespace phosphor::software;
using namespace phosphor::software::example_device;
using SoftwareActivationProgress =
    sdbusplus::aserver::xyz::openbmc_project::software::ActivationProgress<
        Software>;

sdbusplus::async::task<> testDeviceStartUpdateInvalidFD(
    sdbusplus::async::context& ctx)
{
    ExampleCodeUpdater exampleUpdater(ctx, "xyz.openbmc_project.InvalidFD",
                                      "vUnknown");

    auto& device = exampleUpdater.getDevice();

    std::unique_ptr<SoftwareActivationProgress> activationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, "/");

    sdbusplus::message::unix_fd image;
    image.fd = -1;

    EXPECT_FALSE(device->deviceSpecificUpdateFunctionCalled);

    std::unique_ptr<Software> softwareUpdate =
        std::make_unique<Software>(ctx, *device);

    co_await device->startUpdateAsync(image, RequestedApplyTimes::Immediate,
                                      std::move(softwareUpdate));

    // assert the bad file descriptor was caught and we did not proceed
    EXPECT_FALSE(device->deviceSpecificUpdateFunctionCalled);

    ctx.request_stop();

    co_return;
}

TEST(DeviceTest, TestDeviceStartUpdateInvalidFD)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testDeviceStartUpdateInvalidFD(ctx));

    ctx.run();
}
