#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/software_manager.hpp"
#include "fw-update/tool/create_package/create_pldm_fw_package.hpp"
#include "nopdevice.hpp"

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
    testDeviceStartUpdateInvalidFD(sdbusplus::async::context& io)
// NOLINTEND
{
    NopCodeUpdater nopcu(io);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(io, cu);

    device->softwareCurrent =
        std::make_unique<Software>(io, "/", false, *device);

    std::unique_ptr<SoftwareActivationProgress> sap =
        std::make_unique<SoftwareActivationProgress>(io, "/");

    std::string oldswid;
    sdbusplus::message::unix_fd image;
    image.fd = -1;

    const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
        software::ApplyTime::RequestedApplyTimes::Immediate;

    assert(!device->deviceSpecificUpdateFunctionCalled);

    device->startUpdate(image, applyTimeImmediate, oldswid);

    // wait for the update timeout
    co_await sdbusplus::async::sleep_for(
        io, std::chrono::seconds(device->getUpdateTimerDelaySeconds() + 2));

    // assert the bad file descriptor was caught and we did not proceed
    assert(!device->deviceSpecificUpdateFunctionCalled);

    io.request_stop();

    co_return;
}

TEST_F(DeviceTest, TestDeviceStartUpdateInvalidFD)
{
    sdbusplus::async::context io;

    io.spawn(testDeviceStartUpdateInvalidFD(io));

    io.run();
}
