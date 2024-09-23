#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/fw_manager.hpp"
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
    testDeviceSpecificUpdateFunction(sdbusplus::async::context& io)
// NOLINTEND
{
    NopCodeUpdater nopcu(io);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(io, cu);

    // NOLINTBEGIN
    std::unique_ptr<SoftwareActivationProgress> sap =
        std::make_unique<SoftwareActivationProgress>(io, "/");
    uint8_t buffer[10];
    size_t buffer_size = 10;
    bool success = co_await device->deviceSpecificUpdateFunction(
        (const uint8_t*)buffer, buffer_size, sap);

    assert(success);

    // NOLINTEND

    io.request_stop();

    co_return;
}

TEST_F(DeviceTest, TestDeviceConstructor)
{
    sdbusplus::async::context io;
    NopCodeUpdater nopcu(io);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(io, cu);

    assert(device->getEMConfigType() == "Nop");

    // the software version is currently unknown
    assert(device->softwareCurrent == nullptr);

    assert(device->getObjPathConfig() == exampleConfigObjPath);
}

TEST_F(DeviceTest, TestDeviceSpecificUpdateFunction)
{
    sdbusplus::async::context io;

    // NOLINTBEGIN
    io.spawn(testDeviceSpecificUpdateFunction(io));
    // NOLINTEND

    io.run();
}
