#include "device.hpp"

#include "fw-update/common/software.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

Device::Device(sdbusplus::async::context& io, bool isDryRun,
               FWManager* parent) :
    parent(parent), dryRun(isDryRun), bus(io.get_bus()), io(io)
{}

void Device::deleteOldSw(const std::string& swid)
{
    lg2::info("deleting old sw version {SWID}", "SWID", swid);

    // this should also take down the old dbus interfaces

    if (softwareCurrent->swid == swid)
    {
        softwareCurrent = nullptr;
    }
    if (softwareUpdate->swid == swid)
    {
        softwareUpdate = nullptr;
    }
}
