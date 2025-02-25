#include "bios-spi/bios_software_manager.hpp"
#include "bios-spi/spi_device.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Software/Update/server.hpp>

#include <fstream>
#include <iostream>
#include <memory>

#include "gtest/gtest.h"

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<void> start(sdbusplus::async::context& ctx,
                                   BIOSSoftwareManager& spidcu)
// NOLINTEND(readability-static-accessed-through-instance)
{
    uint32_t vendorIANA = 0x0000a015;
    std::string compatible = "com.testvendor.testcomponent";
    auto* cu = &spidcu;
    std::vector<std::string> gpioNames;
    std::vector<uint64_t> gpioValues;

    std::string objPath =
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleDevice";

    SoftwareConfig config(objPath, vendorIANA, compatible, configTypeBIOS,
                          "HostSPI");

    auto sd = std::make_unique<SPIDevice>(
        ctx, 1, 0, true, true, gpioNames, gpioValues, config, cu,
        flashLayoutFlat, flashToolNone, "/tmp/");

    spidcu.devices.insert({objPath, std::move(sd)});

    ctx.request_stop();

    co_return;
}

// NOLINTNEXTLINE(readability-static-accessed-through-instance)
sdbusplus::async::task<void> cancel(sdbusplus::async::context& ctx)
// NOLINTEND(readability-static-accessed-through-instance)
{
    std::cout << "cancel" << std::endl;

    co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(1));

    std::ofstream o("/tmp/cancel_context");
    o << "close" << std::endl;
    o.close();

    co_return;
}

TEST(BIOS, Constructor)
{
    sdbusplus::async::context ctx;
    BIOSSoftwareManager spidcu(ctx, true);

    try
    {
        ctx.spawn(start(ctx, spidcu));
        ctx.spawn(cancel(ctx));
        ctx.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        assert(false);
    }
}
