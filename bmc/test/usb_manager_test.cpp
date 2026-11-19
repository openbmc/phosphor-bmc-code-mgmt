#include "usb/usb_manager.hpp"

#include <sdbusplus/async.hpp>

#include <filesystem>

#include <gtest/gtest.h>

TEST(USBManagerTest, RunReturnsWhenImageCopyFails)
{
    // USBManager keeps references, so these must outlive ctx.run().
    const std::filesystem::path devicePath = "/dev/usb-manager-test-missing";
    const auto usbPath =
        std::filesystem::temp_directory_path() / "usb-manager-test";

    sdbusplus::async::context ctx;
    phosphor::usb::USBManager manager(ctx, devicePath, usbPath);

    ctx.run();

    std::filesystem::remove_all(usbPath);
}
