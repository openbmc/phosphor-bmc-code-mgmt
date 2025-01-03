#include "config.h"

#include "sync_manager.hpp"
#include "sync_watch.hpp"

#include <systemd/sd-event.h>

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

#include <exception>

int main()
{
    auto bus = sdbusplus::bus::new_default();

    sd_event* loop = nullptr;
    sd_event_default(&loop);

    sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);

    try
    {
        using namespace phosphor::software::manager;
        auto syncCallback = std::bind(
            &Sync::processEntry, std::placeholders::_1, std::placeholders::_2);
        phosphor::software::manager::SyncWatch watch(*loop, syncCallback);
        bus.attach_event(loop, SD_EVENT_PRIORITY_NORMAL);
        sd_event_loop(loop);
    }
    catch (const std::exception& e)
    {
        lg2::error("Error in event loop: {ERROR}", "ERROR", e);
        sd_event_unref(loop);
        return -1;
    }

    sd_event_unref(loop);

    return 0;
}
