#include <cstdlib>
#include <exception>
#include <sdbusplus/bus.hpp>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "bmc_version.hpp"
#include "watch.hpp"

int main(int argc, char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    sd_event* loop = nullptr;
    sd_event_default(&loop);

    sdbusplus::server::manager::manager objManager(bus,
            SOFTWARE_OBJPATH);
    phosphor::software::manager::BMCVersion manager(bus,
            SOFTWARE_OBJPATH);
    bus.request_name(BMC_VERSION_BUSNAME);

    try
    {
        phosphor::software::manager::Watch watch(loop, bus);
        bus.attach_event(loop, SD_EVENT_PRIORITY_NORMAL);
        sd_event_loop(loop);
    }
    catch (std::exception& e)
    {
        using namespace phosphor::logging;
        log<level::ERR>(e.what());
        return -1;
    }

    sd_event_unref(loop);

    return 0;
}
