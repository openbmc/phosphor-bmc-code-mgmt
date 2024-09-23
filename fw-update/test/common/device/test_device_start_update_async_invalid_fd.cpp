#include "../nopdevice/nopdevice.hpp"
#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/software_manager.hpp"
#include "fw-update/tool/create_package/create_pldm_fw_package.hpp"

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
    testDeviceStartUpdateInvalidFD(sdbusplus::async::context& ctx)
// NOLINTEND
{
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(ctx, cu);

    device->softwareCurrent =
        std::make_unique<Software>(ctx, "/", "vUnknown", *device);

    std::unique_ptr<SoftwareActivationProgress> sap =
        std::make_unique<SoftwareActivationProgress>(ctx, "/");

    sdbusplus::message::unix_fd image;
    image.fd = -1;

    const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
        software::ApplyTime::RequestedApplyTimes::Immediate;

    assert(!device->deviceSpecificUpdateFunctionCalled);

    const std::string swid = Software::getRandomSoftwareId(*device);

    co_await device->startUpdateAsync(image, applyTimeImmediate, swid);

    // assert the bad file descriptor was caught and we did not proceed
    assert(!device->deviceSpecificUpdateFunctionCalled);

    ctx.request_stop();

    co_return;
}

TEST_F(DeviceTest, TestDeviceStartUpdateInvalidFD)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testDeviceStartUpdateInvalidFD(ctx));

    ctx.run();
}
