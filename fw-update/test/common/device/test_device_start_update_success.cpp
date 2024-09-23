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
    testDeviceStartUpdateSuccess(sdbusplus::async::context& io)
// NOLINTEND
{
    NopCodeUpdater nopcu(io);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(io, cu);

    device->softwareCurrent =
        std::make_unique<Software>(io, "/", false, *device);

    std::unique_ptr<SoftwareActivationProgress> sap =
        std::make_unique<SoftwareActivationProgress>(io, "/");

    const int fd = memfd_create("test_memfd", MFD_CLOEXEC);

    assert(fd >= 0);

    uint8_t component_image[] = {0x12, 0x34, 0x83, 0x21};

    size_t size_out = 0;
    std::shared_ptr<uint8_t[]> buf = create_pldm_package_buffer(
        component_image, sizeof(component_image),
        std::optional<uint32_t>(exampleVendorIANA),
        std::optional<std::string>(exampleCompatible), &size_out);

    ssize_t bytes_written = write(fd, (void*)buf.get(), size_out);
    if (bytes_written == -1)
    {
        std::cerr << "Failed to write to memfd: " << strerror(errno)
                  << std::endl;
        close(fd);
        assert(false);
    }
    lseek(fd, 0, SEEK_SET);

    std::string oldswid;
    sdbusplus::message::unix_fd image;
    image.fd = fd;

    const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
        software::ApplyTime::RequestedApplyTimes::Immediate;

    device->startUpdate(image, applyTimeImmediate, oldswid);

    // TODO: do not rely on hardcoded timeout. we should get a handle to the
    // processing coroutine and await or some other solution

    // wait for the update timeout, plus some more time to process
    co_await sdbusplus::async::sleep_for(
        io, std::chrono::seconds(device->getUpdateTimerDelaySeconds() + 2));

    assert(device->deviceSpecificUpdateFunctionCalled);

    close(fd);

    io.request_stop();

    co_return;
}

TEST_F(DeviceTest, TestDeviceStartUpdateSuccess)
{
    sdbusplus::async::context io;

    io.spawn(testDeviceStartUpdateSuccess(io));

    io.run();
}
