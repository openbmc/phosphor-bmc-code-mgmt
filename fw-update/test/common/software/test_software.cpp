#include "fw-update/common/include/device.hpp"
#include "fw-update/common/include/software_manager.hpp"
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

class SoftwareTest : public testing::Test
{
    void SetUp() override {}
    void TearDown() override {}
};

// NOLINTBEGIN
sdbusplus::async::task<> endContext(sdbusplus::async::context& io)
// NOLINTEND
{
    io.request_stop();

    co_return;
}

TEST_F(SoftwareTest, TestSoftwareConstructor)
{
    sdbusplus::async::context io;
    NopCodeUpdater nopcu(io);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(io, cu);

    // the software version is currently unknown
    assert(device->softwareCurrent == nullptr);

    auto sw = std::make_unique<Software>(io, "/", true, *device);

    // since that software is not an update, there is no progress
    assert(sw->optSoftwareActivationProgress == nullptr);

    // test succeeds if we construct without any exception
    // and can run the io context

    io.spawn(endContext(io));
    io.run();
}

TEST_F(SoftwareTest, TestSoftwareId)
{
    sdbusplus::async::context io;
    NopCodeUpdater nopcu(io);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(io, cu);

    // the software version is currently unknown
    assert(device->softwareCurrent == nullptr);

    auto sw = std::make_unique<Software>(io, "/", true, *device);

    // design: Swid = <DeviceX>_<RandomId>
    assert(sw->swid.starts_with("Nop_"));
}

TEST_F(SoftwareTest, TestSoftwareObjectPath)
{
    sdbusplus::async::context io;
    NopCodeUpdater nopcu(io);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(io, cu);

    auto sw = std::make_unique<Software>(io, "/", true, *device);

    lg2::debug("{PATH}", "PATH", sw->getObjectPath());

    // assert that the object path is as per the design
    // design: /xyz/openbmc_project/Software/<SwId>
    assert(std::string(sw->getObjectPath())
               .starts_with("/xyz/openbmc_project/software/"));
}
