
#include "exampledevice/example_device.hpp"
#include "test/create_package/create_pldm_fw_package.hpp"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>
#include <xyz/openbmc_project/Association/Definitions/client.hpp>
#include <xyz/openbmc_project/Software/Update/client.hpp>
#include <xyz/openbmc_project/Software/Version/client.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>

// NOLINTBEGIN
sdbusplus::async::task<> testSoftwareUpdateSuccess(
    sdbusplus::async::context& ctx, int fd)
// NOLINTEND
{
    ExampleCodeUpdater nopcu(ctx);
    ExampleCodeUpdater* cu = &nopcu;

    const std::string service =
        "xyz.openbmc_project.Software.NopTestSoftwareUpdateSuccess";
    ctx.get_bus().request_name(service.c_str());

    auto device = std::make_unique<ExampleDevice>(ctx, cu);

    device->softwareCurrent = std::make_unique<Software>(ctx, *device);

    device->softwareCurrent->setVersion("v12.345");

    std::unique_ptr<SoftwareActivationProgress> sap =
        std::make_unique<SoftwareActivationProgress>(ctx, "/");

    const auto applyTimeImmediate = sdbusplus::common::xyz::openbmc_project::
        software::ApplyTime::RequestedApplyTimes::Immediate;

    device->softwareCurrent->enableUpdate({applyTimeImmediate});

    std::string objPathCurrentSoftware =
        device->softwareCurrent->getObjectPath();

    // go via dbus to call the dbus method to start the update
    auto client =
        sdbusplus::client::xyz::openbmc_project::software::Update<>(ctx)
            .service(service)
            .path(objPathCurrentSoftware);

    sdbusplus::message::object_path objPathNewSoftware =
        co_await client.start_update(fd, applyTimeImmediate);

    // TODO: somehow remove this sleep
    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(2));

    assert(objPathNewSoftware != objPathCurrentSoftware);

    // assert that update function was called
    assert(device->deviceSpecificUpdateFunctionCalled);

    auto clientNewVersion =
        sdbusplus::client::xyz::openbmc_project::software::Version<>(ctx)
            .service(service)
            .path(objPathNewSoftware.str);

    const std::string newVersion = co_await clientNewVersion.version();

    // assert the new version is not the old version
    assert(newVersion != "v12.345");
    assert(newVersion == "VersionString1");

    auto clientAssoc =
        sdbusplus::client::xyz::openbmc_project::association::Definitions<>(ctx)
            .service(service)
            .path(objPathNewSoftware.str);

    // assert that the new version is associated as the running version
    {
        auto res = co_await clientAssoc.associations();
        assert(res.size() == 1);

        auto assoc = res[0];

        std::string forward = std::get<0>(assoc);
        std::string reverse = std::get<1>(assoc);
        std::string endpoint = std::get<2>(assoc);

        assert(forward == "running");
        assert(reverse == "ran_on");
        assert(endpoint == (co_await device->getInventoryItemObjectPath()));
    }

    ctx.request_stop();

    co_return;
}

TEST(SoftwareUpdate, TestSoftwareUpdate)
{
    sdbusplus::async::context ctx;

    int fd = memfd_create("test_memfd", 0);

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

    ctx.spawn(testSoftwareUpdateSuccess(ctx, fd));

    ctx.run();

    close(fd);
}
