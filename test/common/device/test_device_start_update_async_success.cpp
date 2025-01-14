#include "../nopdevice/nopdevice.hpp"
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

// NOLINTBEGIN
sdbusplus::async::task<>
    testDeviceStartUpdateSuccess(sdbusplus::async::context& ctx)
// NOLINTEND
{
    NopCodeUpdater nopcu(ctx);
    NopCodeUpdater* cu = &nopcu;

    auto device = std::make_unique<NopDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, "swid1", *device);

    device->softwareCurrent->setVersion("vUnknown");

    std::unique_ptr<SoftwareActivationProgress> sap =
        std::make_unique<SoftwareActivationProgress>(ctx, "/");

    const int fd = memfd_create("test_memfd", 0);

    assert(fd >= 0);

    lg2::debug("create fd {FD}", "FD", fd);

    uint8_t component_image[] = {0x12, 0x34, 0x83, 0x21};

    size_t size_out = 0;
    std::unique_ptr<uint8_t[]> buf = create_pldm_package_buffer(
        component_image, sizeof(component_image),
        std::optional<uint32_t>(exampleVendorIANA),
        std::optional<std::string>(exampleCompatible), size_out);

    ssize_t bytes_written = write(fd, (void*)buf.get(), size_out);
    if (bytes_written == -1)
    {
        std::cerr << "Failed to write to memfd: " << strerror(errno)
                  << std::endl;
        close(fd);
        assert(false);
    }
    if (lseek(fd, 0, SEEK_SET) != 0)
    {
        lg2::error("could not seek to the beginning of the file");
        assert(false);
    }

    int fd2 = dup(fd);
    sdbusplus::message::unix_fd image2 = fd2;

    if (fd2 < 0)
    {
        lg2::error("ERROR calling dup on fd: {ERR}", "ERR", strerror(errno));
        assert(false);
    }

    lg2::debug("dup fd: {FD}", "FD", fd2);

    const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
        software::ApplyTime::RequestedApplyTimes::Immediate;

    std::unique_ptr<Software> softwareUpdate =
        std::make_unique<Software>(ctx, "myotherswid", *device);

    co_await device->startUpdateAsync(image2, applyTimeImmediate,
                                      std::move(softwareUpdate));

    assert(device->deviceSpecificUpdateFunctionCalled);

    close(fd);

    ctx.request_stop();

    co_return;
}

TEST(DeviceTest, TestDeviceStartUpdateSuccess)
{
    sdbusplus::async::context ctx;

    ctx.spawn(testDeviceStartUpdateSuccess(ctx));

    ctx.run();
}
