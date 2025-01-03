#include "config.h"

#include "image_manager.hpp"
#include "watch.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <cstdlib>
#include <exception>

int main()
{
    using namespace phosphor::software::manager;
    auto bus = sdbusplus::bus::new_default();

    sd_event* loop = nullptr;
    sd_event_default(&loop);

    sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);
    bus.request_name(VERSION_BUSNAME);

    try
    {
        phosphor::software::manager::Manager imageManager(bus);
        phosphor::software::manager::Watch watch(
            loop, std::bind(std::mem_fn(&Manager::processImage), &imageManager,
                            std::placeholders::_1));
        bus.attach_event(loop, SD_EVENT_PRIORITY_NORMAL);
        sd_event_loop(loop);
    }
    catch (const std::exception& e)
    {
        lg2::error("Error in event loop: {ERROR}", "ERROR", e);
        return -1;
    }

    sd_event_unref(loop);

    return 0;
}
