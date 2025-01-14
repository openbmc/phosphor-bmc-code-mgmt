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

class DeviceTest : public testing::Test
{
  protected:
    DeviceTest() :
        exampleUpdater(ctx, "xyz.openbmc_project.ExampleUpdater", "vUnknown"),
        device(exampleUpdater.getDevice())
    {}
    ~DeviceTest() noexcept override {}

    sdbusplus::async::context ctx;
    ExampleCodeUpdater exampleUpdater;
    std::unique_ptr<ExampleDevice>& device;

  public:
    DeviceTest(const DeviceTest&) = delete;
    DeviceTest(DeviceTest&&) = delete;
    DeviceTest& operator=(const DeviceTest&) = delete;
    DeviceTest& operator=(DeviceTest&&) = delete;
};

sdbusplus::async::task<> testDeviceStartUpdateImmediateSuccess(
    sdbusplus::async::context& ctx, std::unique_ptr<ExampleDevice>& device)
{
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

TEST_F(DeviceTest, TestDeviceStartUpdateImmediateSuccess)
{
    ctx.spawn(testDeviceStartUpdateImmediateSuccess(ctx, device));
    ctx.run();
}

sdbusplus::async::task<> testDeviceStartUpdateOnResetSuccess(
    sdbusplus::async::context& ctx, ExampleCodeUpdater& exampleUpdater,
    std::unique_ptr<ExampleDevice>& device)
{
    const Software* oldCurrent =
        exampleUpdater.getDevice()->softwareCurrent.get();

    std::unique_ptr<SoftwareActivationProgress> activationProgress =
        std::make_unique<SoftwareActivationProgress>(ctx, "/");

    const int fd = memfd_create("test_memfd", 0);

    EXPECT_GE(fd, 0);

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

    const Software* updateSoftware = softwareUpdate.get();

    co_await device->startUpdateAsync(image2, RequestedApplyTimes::OnReset,
                                      std::move(softwareUpdate));

    EXPECT_TRUE(device->deviceSpecificUpdateFunctionCalled);

    // assert that the old software is still the running version,
    // since the apply time is 'OnReset'
    EXPECT_EQ(device->softwareCurrent.get(), oldCurrent);

    // assert that the updated software is present
    EXPECT_EQ(device->softwarePending.get(), updateSoftware);

    close(fd);

    ctx.request_stop();

    co_return;
}

TEST_F(DeviceTest, TestDeviceStartUpdateOnResetSuccess)
{
    ctx.spawn(testDeviceStartUpdateOnResetSuccess(ctx, exampleUpdater, device));
    ctx.run();
}

sdbusplus::async::task<> testDeviceStartUpdateInvalidFD(
    sdbusplus::async::context& ctx, std::unique_ptr<ExampleDevice>& device)
{
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

TEST_F(DeviceTest, TestDeviceStartUpdateInvalidFD)
{
    ctx.spawn(testDeviceStartUpdateInvalidFD(ctx, device));
    ctx.run();
}

sdbusplus::async::task<> testDeviceSpecificUpdateFunction(
    sdbusplus::async::context& ctx, std::unique_ptr<ExampleDevice>& device)
{
    uint8_t buffer[10];
    size_t buffer_size = 10;

    auto previousVersion = device->softwareCurrent->swid;

    bool success =
        co_await device->updateDevice((const uint8_t*)buffer, buffer_size);

    EXPECT_TRUE(success);

    EXPECT_NE(device->softwareCurrent, nullptr);
    EXPECT_EQ(device->softwarePending, nullptr);

    ctx.request_stop();

    co_return;
}

TEST_F(DeviceTest, TestDeviceSpecificUpdateFunction)
{
    // NOLINTBEGIN(clang-analyzer-core.uninitialized.Branch)
    ctx.spawn(testDeviceSpecificUpdateFunction(ctx, device));
    // NOLINTEND(clang-analyzer-core.uninitialized.Branch)
    ctx.run();
}

TEST_F(DeviceTest, TestDeviceConstructor)
{
    EXPECT_TRUE(device->getEMConfigType().starts_with("Nop"));

    // the software version is initialized
    EXPECT_NE(device->softwareCurrent, nullptr);

    // there is no pending update
    EXPECT_EQ(device->softwarePending, nullptr);
}
