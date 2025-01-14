#include "../exampledevice/example_device.hpp"
#include "test/create_package/create_pldm_fw_package.hpp"

#include <sys/mman.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <memory>

#include <gtest/gtest.h>

PHOSPHOR_LOG2_USING;

using namespace phosphor::software;
using namespace phosphor::software::example_device;
using SoftwareActivationProgress =
    sdbusplus::aserver::xyz::openbmc_project::software::ActivationProgress<
        Software>;

sdbusplus::async::task<> testDeviceStartUpdateImmediateSuccess(
    sdbusplus::async::context& ctx)
{
    ExampleCodeUpdater exampleUpdater(
        ctx, "xyz.openbmc_project.UpdateImmediateSuccess", "vUnknown");

    auto& device = exampleUpdater.getDevice();

    const Software* oldSoftware = device->softwareCurrent.get();

    std::unique_ptr<SoftwareActivationProgress> activationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, "/");

    const int fd = memfd_create("test_memfd", 0);

    EXPECT_TRUE(fd >= 0);

    debug("create fd {FD}", "FD", fd);

    uint8_t component_image[] = {0x12, 0x34, 0x83, 0x21};

    size_t size_out = 0;
    std::unique_ptr<uint8_t[]> buf = create_pldm_package_buffer(
        component_image, sizeof(component_image),
        std::optional<uint32_t>(exampleVendorIANA),
        std::optional<std::string>(exampleCompatibleHardware), size_out);

    ssize_t bytes_written = write(fd, (void*)buf.get(), size_out);
    if (bytes_written == -1)
    {
        std::cerr << "Failed to write to memfd: " << strerror(errno)
                  << std::endl;
        close(fd);
        EXPECT_TRUE(false);
    }
    if (lseek(fd, 0, SEEK_SET) != 0)
    {
        error("could not seek to the beginning of the file");
        EXPECT_TRUE(false);
    }

    int fd2 = dup(fd);
    sdbusplus::message::unix_fd image2 = fd2;

    if (fd2 < 0)
    {
        error("ERROR calling dup on fd: {ERR}", "ERR", strerror(errno));
        EXPECT_TRUE(false);
    }

    debug("dup fd: {FD}", "FD", fd2);

    std::unique_ptr<Software> softwareUpdate =
        std::make_unique<Software>(ctx, *device);

    const Software* newSoftware = softwareUpdate.get();

    co_await device->startUpdateAsync(image2, RequestedApplyTimes::Immediate,
                                      std::move(softwareUpdate));

    EXPECT_TRUE(device->deviceSpecificUpdateFunctionCalled);

    EXPECT_NE(device->softwareCurrent.get(), oldSoftware);
    EXPECT_EQ(device->softwareCurrent.get(), newSoftware);

    close(fd);

    ctx.request_stop();

    co_return;
}

TEST(DeviceTest, TestDeviceStartUpdateImmediateSuccess)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testDeviceStartUpdateImmediateSuccess(ctx));

    ctx.run();
}
